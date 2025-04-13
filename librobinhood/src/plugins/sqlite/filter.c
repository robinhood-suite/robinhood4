/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

#include <robinhood/statx.h>

#include <stdarg.h>

struct sqlite_filter_where {
    char clause[2048];
    size_t clause_len;
};

__attribute__((format(printf, 2, 3)))
static bool
sfw_clause_format(struct sqlite_filter_where *where, const char *fmt, ...)
{
    va_list args;
    int len;

    va_start(args, fmt);
    len = vsnprintf(where->clause + where->clause_len,
                    sizeof(where->clause) - where->clause_len,
                    fmt, args);
    va_end(args);
    if (len == -1)
        return sqlite_fail("vsnprintf: failed to format characters");

    if (len > (sizeof(where->clause) - where->clause_len))
        return sqlite_fail("vsnprintf: truncated string, buffer too small");

    where->clause_len += len;
    return true;
}

const struct rbh_id ROOT_ID = {
    .data = NULL,
    .size = 0,
};

static void *
sqlite_iter_next(void *iterator)
{
    struct sqlite_iterator *iter = iterator;
    struct rbh_value_map inode_xattrs = {0};
    struct rbh_value_map ns_xattrs = {0};
    const char *inode_xattrs_json;
    struct sqlite_cursor *cursor;
    struct rbh_fsentry tmp = {0};
    struct rbh_fsentry *fsentry;
    const char *ns_xattrs_json;
    const char *symlink = NULL;
    struct rbh_statx statx;
    int save_errno;

    cursor = &iter->cursor;

    if (iter->done) {
        errno = ENODATA;
        return NULL;
    }

    if (!sqlite_cursor_step(cursor))
        return NULL;

    if (errno == 0) {
        /* sqlite_cursor_step returns true with errno = EAGAIN if more rows
         * are availble. errno = 0 means that their is no more data to read.
         */
        errno = ENODATA;
        /* sqlite3_step seems to go back to the beginning if called again after
         * having stepped through all the rows. Set done to true to remember the
         * first time we've reached the end.
         */
        iter->done = true;
        return NULL;
    }

    if (!(sqlite_cursor_get_id(cursor, &tmp.id) &&
          sqlite_cursor_get_id(cursor, &tmp.parent_id)))
        return NULL;

    tmp.name = sqlite_cursor_get_string(cursor);

    statx.stx_mask = sqlite_cursor_get_uint32(cursor);
    statx.stx_blksize = sqlite_cursor_get_uint32(cursor);

    statx.stx_nlink = sqlite_cursor_get_uint32(cursor);
    statx.stx_uid = sqlite_cursor_get_uint32(cursor);
    statx.stx_gid = sqlite_cursor_get_uint32(cursor);
    statx.stx_mode =
        sqlite_cursor_get_uint16(cursor) |
        sqlite_cursor_get_uint16(cursor);

    statx.stx_ino = sqlite_cursor_get_uint64(cursor);
    statx.stx_size = sqlite_cursor_get_uint64(cursor);
    statx.stx_blocks = sqlite_cursor_get_uint64(cursor);
    statx.stx_attributes = sqlite_cursor_get_uint64(cursor);
    statx.stx_atime.tv_sec = sqlite_cursor_get_int64(cursor);

    statx.stx_atime.tv_nsec = sqlite_cursor_get_uint32(cursor);
    statx.stx_btime.tv_sec = sqlite_cursor_get_int64(cursor);
    statx.stx_btime.tv_nsec = sqlite_cursor_get_uint32(cursor);
    statx.stx_ctime.tv_sec = sqlite_cursor_get_int64(cursor);
    statx.stx_ctime.tv_nsec = sqlite_cursor_get_uint32(cursor);

    statx.stx_mtime.tv_sec = sqlite_cursor_get_int64(cursor);
    statx.stx_mtime.tv_nsec = sqlite_cursor_get_uint32(cursor);
    statx.stx_rdev_major = sqlite_cursor_get_uint32(cursor);
    statx.stx_rdev_minor = sqlite_cursor_get_uint32(cursor);
    statx.stx_dev_major = sqlite_cursor_get_uint32(cursor);

    statx.stx_dev_minor = sqlite_cursor_get_uint32(cursor);
    statx.stx_mnt_id = sqlite_cursor_get_uint64(cursor);

    inode_xattrs_json = sqlite_cursor_get_string(cursor);
    ns_xattrs_json = sqlite_cursor_get_string(cursor);
    if (!(sqlite_json2xattrs(inode_xattrs_json, &inode_xattrs) &&
          sqlite_json2xattrs(ns_xattrs_json, &ns_xattrs))) {
        fsentry = NULL;
        goto out_free;
    }
    symlink = sqlite_cursor_get_string(cursor);

    errno = ENODATA;
    fsentry = rbh_fsentry_new(&tmp.id,
                              tmp.parent_id.size > 0 ?
                                &tmp.parent_id :
                                &ROOT_ID,
                              tmp.name,
                              &statx,
                              &ns_xattrs,
                              &inode_xattrs,
                              symlink);

out_free:
    save_errno = errno;
    free((void *)tmp.name);
    free((void *)tmp.id.data);
    free((void *)tmp.parent_id.data);
    free((void *)symlink);
    errno = save_errno;

    return fsentry;
}

static void
sqlite_iter_destroy(void *iterator)
{
    struct sqlite_iterator *iter = iterator;

    if (iter->cursor.stmt)
        sqlite_cursor_fini(&iter->cursor);
    free(iter);
}

static const struct rbh_mut_iterator_operations SQLITE_ITER_OPS = {
    .next    = sqlite_iter_next,
    .destroy = sqlite_iter_destroy,
};

static const struct rbh_mut_iterator SQLITE_ITER = {
    .ops = &SQLITE_ITER_OPS,
};

static struct sqlite_iterator *
sqlite_iterator_new(void)
{
    struct sqlite_iterator *iter;

    iter = malloc(sizeof(*iter));
    if (!iter)
        return NULL;

    iter->iter = SQLITE_ITER;
    iter->cursor.stmt = NULL;
    iter->done = false;

    return iter;
}

static bool
filter2sql(const struct rbh_filter *filter, struct sqlite_filter_where *where,
           size_t depth, bool negate);

const char *int64_ops[] = {
    [RBH_FOP_EQUAL]            = "%s = ?",
    [RBH_FOP_STRICTLY_LOWER]   = "%s < ?",
    [RBH_FOP_LOWER_OR_EQUAL]   = "%s <= ?",
    [RBH_FOP_STRICTLY_GREATER] = "%s > ?",
    [RBH_FOP_GREATER_OR_EQUAL] = "%s >= ?",
    [RBH_FOP_REGEX]            = NULL,
    [RBH_FOP_IN]               = NULL,
    [RBH_FOP_EXISTS]           = NULL,
    [RBH_FOP_BITS_ANY_SET]     = "bit_any_set(%s, ?)",
    [RBH_FOP_BITS_ALL_SET]     = "bit_all_set(%s, ?)",
    [RBH_FOP_BITS_ANY_CLEAR]   = "bit_any_clear(%s, ?)",
    [RBH_FOP_BITS_ALL_CLEAR]   = "bit_all_clear(%s, ?)",
};

const char *neg_int64_ops[] = {
    [RBH_FOP_EQUAL]            = "%s <> ?",
    [RBH_FOP_STRICTLY_LOWER]   = "%s >= ?",
    [RBH_FOP_LOWER_OR_EQUAL]   = "%s > ?",
    [RBH_FOP_STRICTLY_GREATER] = "%s <= ?",
    [RBH_FOP_GREATER_OR_EQUAL] = "%s < ?",
    [RBH_FOP_REGEX]            = NULL,
    [RBH_FOP_IN]               = NULL,
    [RBH_FOP_EXISTS]           = NULL,
    [RBH_FOP_BITS_ANY_SET]     = "bit_all_clear(%s, ?)",
    [RBH_FOP_BITS_ALL_SET]     = "bit_any_clear(%s, ?)",
    [RBH_FOP_BITS_ANY_CLEAR]   = "bit_all_set(%s, ?)",
    [RBH_FOP_BITS_ALL_CLEAR]   = "bit_any_set(%s, ?)",
};

static const char *
filter2op(const struct rbh_filter *filter, bool negate)
{
    if (negate) {
        switch (filter->compare.value.type) {
        case RBH_VT_BOOLEAN:
        case RBH_VT_INT32:
        case RBH_VT_UINT32:
        case RBH_VT_INT64:
        case RBH_VT_UINT64:
            return neg_int64_ops[filter->op];

        case RBH_VT_BINARY:
            if (filter->compare.value.binary.size == 0)
                return "%s <> x''";
        case RBH_VT_STRING:
            return "%s <> ?";
        case RBH_VT_REGEX:
        case RBH_VT_SEQUENCE:
        case RBH_VT_MAP:
            return NULL;
        }
    } else {
        switch (filter->compare.value.type) {
        case RBH_VT_BOOLEAN:
        case RBH_VT_INT32:
        case RBH_VT_UINT32:
        case RBH_VT_INT64:
        case RBH_VT_UINT64:
            return int64_ops[filter->op];

        case RBH_VT_BINARY:
            if (filter->compare.value.binary.size == 0)
                return "%s = x''";
        case RBH_VT_STRING:
            return "%s = ?";
        case RBH_VT_REGEX:
        case RBH_VT_SEQUENCE:
        case RBH_VT_MAP:
            return NULL;
        }
    }
    __builtin_unreachable();
}

static const char *
statx_field2str(uint32_t value)
{
    if (value & RBH_STATX_TYPE)
        return "type";
    else if (value & RBH_STATX_MODE)
        return "mode";
    else if (value & RBH_STATX_NLINK)
        return "nlink";
    else if (value & RBH_STATX_UID)
        return "uid";
    else if (value & RBH_STATX_GID)
        return "gid";
    else if (value & RBH_STATX_ATIME_SEC)
        return "atime_sec";
    else if (value & RBH_STATX_MTIME_SEC)
        return "mtime_sec";
    else if (value & RBH_STATX_CTIME_SEC)
        return "ctime_sec";
    else if (value & RBH_STATX_INO)
        return "ino";
    else if (value & RBH_STATX_SIZE)
        return "size";
    else if (value & RBH_STATX_BLOCKS)
        return "blocks";
    else if (value & RBH_STATX_BTIME_SEC)
        return "btime_sec";
    else if (value & RBH_STATX_MNT_ID)
        return "mnt_id";
    else if (value & RBH_STATX_BLKSIZE)
        return "blksize";
    else if (value & RBH_STATX_ATTRIBUTES)
        return "attributes";
    else if (value & RBH_STATX_ATIME_NSEC)
        return "atime_nsec";
    else if (value & RBH_STATX_BTIME_NSEC)
        return "btime_nsec";
    else if (value & RBH_STATX_CTIME_NSEC)
        return "ctime_nsec";
    else if (value & RBH_STATX_MTIME_NSEC)
        return "mtime_nsec";
    else if (value & RBH_STATX_RDEV_MAJOR)
        return "rdev_major";
    else if (value & RBH_STATX_RDEV_MINOR)
        return "rdev_minor";
    else if (value & RBH_STATX_DEV_MAJOR)
        return "dev_major";
    else if (value & RBH_STATX_DEV_MINOR)
        return "dev_minor";
    else if (value & RBH_STATX_ATIME) // FIXME see if this is correct
        return "atime_sec";
    else if (value & RBH_STATX_BTIME)
        return "btime_sec";
    else if (value & RBH_STATX_CTIME)
        return "ctime_sec";
    else if (value & RBH_STATX_MTIME)
        return "mtime_sec";
    else if (value & RBH_STATX_RDEV)
        return "rdev";
    else if (value & RBH_STATX_DEV)
        return "dev";
    else
        return NULL;
}

static const char *
field2str(const struct rbh_filter_field *field, bool case_insensitive)
{
    switch (field->fsentry) {
    case RBH_FP_ID:
        return "entries.id";
    case RBH_FP_PARENT_ID:
        return "parent_id";
    case RBH_FP_NAME:
        return "name";
    case RBH_FP_STATX:
        return statx_field2str(field->statx);
    case RBH_FP_SYMLINK:
        return "symlink";
    case RBH_FP_NAMESPACE_XATTRS:
        return "ns.xattrs";
    case RBH_FP_INODE_XATTRS:
        return "entries.xattrs";
    }
    return NULL;
}

static bool
in_array_filter(struct sqlite_filter_where *where,
                const struct rbh_filter *filter)
{
    if (!sfw_clause_format(where, "parent_id in (?"))
        return false;

    switch (filter->compare.value.type) {
    case RBH_VT_SEQUENCE:
        for (size_t i = 0; i < filter->compare.value.sequence.count - 1; i++) {
            if (!sfw_clause_format(where, ", ?"))
                return false;
        }
        break;
    default:
        break;
    }

    return sfw_clause_format(where, ")");
}

static bool
filter_is_case_insensitive(const struct rbh_filter *filter)
{
    return filter->compare.value.type == RBH_VT_REGEX &&
        (filter->compare.value.regex.options & RBH_RO_CASE_INSENSITIVE);
}

static bool
comparison_filter2sql(const struct rbh_filter *filter,
                      struct sqlite_filter_where *where,
                      size_t depth, bool negate, const char *_field)
{
    const char *field = field2str(&filter->compare.field,
                                  filter_is_case_insensitive(filter));
    bool res = false;
    const char *op;

    switch (filter->op) {
    case RBH_FOP_EQUAL:
    case RBH_FOP_STRICTLY_LOWER:
    case RBH_FOP_LOWER_OR_EQUAL:
    case RBH_FOP_STRICTLY_GREATER:
    case RBH_FOP_BITS_ANY_SET:
    case RBH_FOP_BITS_ALL_SET:
    case RBH_FOP_BITS_ALL_CLEAR:
    case RBH_FOP_BITS_ANY_CLEAR:
        op = filter2op(filter, negate);
        if (filter->compare.field.fsentry == RBH_FP_INODE_XATTRS ||
            filter->compare.field.fsentry == RBH_FP_NAMESPACE_XATTRS) {
            char fmt[64];
            int len;

            if (!_field) {
                len = snprintf(fmt, sizeof(fmt), op, "tmp.value");
                if (len == -1 || len > sizeof(fmt))
                    return sqlite_fail("failed to format xattr field");

                res = sfw_clause_format(where,
                                        "exists ("
                                        "  select 1"
                                        "  from json_each(%s, ?) as tmp"
                                        "  where %s"
                                        ")",
                                        field, fmt);
            } else {
                res = sfw_clause_format(where, op, _field);
            }
        } else {
            res = sfw_clause_format(where, op, field);
        }
        break;
    case RBH_FOP_REGEX:
        if (_field)
            field = _field;

        switch (filter->compare.field.fsentry) {
            case RBH_FP_NAME:
            case RBH_FP_SYMLINK:
                if (negate)
                    res = sfw_clause_format(where, "%s is null or not %s regexp ?",
                                            field, field);
                else
                    res = sfw_clause_format(where, "%s is not null and %s regexp ?",
                                            field, field);
                break;
            default:
                res = sfw_clause_format(where,
                                        "%sexists ("
                                        "  select 1"
                                        "  from json_each(%s, ?) as tmp"
                                        "  where tmp.value regexp ?"
                                        ")",
                                        negate ? "not " : "",
                                        field);
                break;
        }
        break;
    case RBH_FOP_IN:
        if (_field)
            field = _field;

        switch (filter->compare.field.fsentry) {
        case RBH_FP_INODE_XATTRS:
        case RBH_FP_NAMESPACE_XATTRS:
            res = sfw_clause_format(where,
                                    "exists ("
                                    "    select 1"
                                    "    from json_each(%s, '$.%s') as tmp"
                                    "    where tmp.value %s ?"
                                    ")",
                                    field,
                                    filter->compare.field.xattr,
                                    negate ? "<>" : "=");
            break;
        case RBH_FP_PARENT_ID:
            res = in_array_filter(where, filter);
            break;
        default:
            abort();
        }
        break;
    case RBH_FOP_EXISTS:
        if (_field)
            field = _field;

        if (!negate)
            res = sfw_clause_format(where, "json_extract(%s, ?) is not null",
                                    field);
        else
            res = sfw_clause_format(where, "json_extract(%s, ?) is null",
                                    field);
        break;
    default:
        abort();
    }

    return res;
}

static bool
logical_filter_has_non_null_filter(const struct rbh_filter *filter)
{
    for (size_t i = 0; i < filter->logical.count; i++)
        if (filter->logical.filters[i])
            return true;

    return false;
}

static bool
empty_filter(const struct rbh_filter *filter)
{
    if (!filter)
        return true;

    return (rbh_is_logical_operator(filter->op) ?
                !logical_filter_has_non_null_filter(filter) :
                false);
}

static bool
and2sql(const struct rbh_filter *filter, struct sqlite_filter_where *where,
        size_t depth, bool negate)
{
    if (negate) {
        if (!sfw_clause_format(where, "not ("))
            return false;
    }

    for (size_t i = 0; i < filter->logical.count; i++) {
        if (!filter->logical.filters[i])
            continue;

        if (!filter2sql(filter->logical.filters[i], where, depth, false))
            return false;

        if (i != filter->logical.count - 1 &&
            !empty_filter(filter->logical.filters[i + 1]))
            sfw_clause_format(where, " and ");
    }
    if (negate) {
        if (!sfw_clause_format(where, ")"))
            return false;
    }

    return true;
}

static bool
or2sql(const struct rbh_filter *filter, struct sqlite_filter_where *where,
       size_t depth, bool negate)
{
    bool needs_paren = false;

    if (negate) {
        if (!sfw_clause_format(where, "not ("))
            return false;
    }

    for (size_t i = 0; i < filter->logical.count; i++) {
        if (!filter->logical.filters[i])
            continue;

        if (!filter2sql(filter->logical.filters[i], where,
                        depth + (needs_paren ? 1 : 0),
                        false))
            return false;

        if (i != filter->logical.count - 1 &&
            !empty_filter(filter->logical.filters[i + 1]))
            sfw_clause_format(where, " or ");
    }

    if (negate) {
        if (!sfw_clause_format(where, ")"))
            return false;
    }

    return true;
}

static bool
elemmatch2sql(const struct rbh_filter *filter,
              struct sqlite_filter_where *where,
              size_t depth, bool negate)
{
    const char *field = field2str(&filter->array.field, false);

    if (filter->array.count == 0)
        return sqlite_fail("not elements found in elemmatch filter");

    if (!sfw_clause_format(where,
                           "exists ("
                           "  select 1"
                           "  from json_each(%s, ?) as em_tmp"
                           "  where ",
                           field))
        return false;

    for (size_t i = 0; i < filter->array.count; i++) {
        const struct rbh_filter *subfilter = filter->array.filters[i];

        if (!rbh_is_comparison_operator(subfilter->op))
            return sqlite_fail("'elemmatch' only support comparision filters, got '%d'",
                               subfilter->op);

        if (!comparison_filter2sql(subfilter, where, depth, negate,
                                   "em_tmp.value"))
            return false;

        if (i != filter->array.count - 1 && !sfw_clause_format(where, " and "))
            return false;
    }

    return sfw_clause_format(where, ")");
}

static bool
logical_filter2sql(const struct rbh_filter *filter,
                   struct sqlite_filter_where *where,
                   size_t depth, bool negate)
{
    switch (filter->op) {
    case RBH_FOP_AND:
        return and2sql(filter, where, depth, negate);
    case RBH_FOP_NOT:
        return filter2sql(filter->logical.filters[0], where, depth, !negate);
    case RBH_FOP_OR:
        return or2sql(filter, where, depth, negate);
    case RBH_FOP_GET:
        return filter2sql(filter->get.filter, where, depth, negate);
    case RBH_FOP_ELEMMATCH:
        return elemmatch2sql(filter, where, depth, negate);
    default:
        abort();
    }
    return false;
}

static bool
filter2sql(const struct rbh_filter *filter, struct sqlite_filter_where *where,
           size_t depth, bool negate)
{
    return (depth > 0 ? sfw_clause_format(where, "(") : true) &&
        (rbh_is_comparison_operator(filter->op) ?
         comparison_filter2sql(filter, where, depth, negate, NULL) :
         logical_filter2sql(filter, where, depth, negate)) &&
        (depth > 0 ? sfw_clause_format(where, ")") : true);
}

static bool
filter2where_clause(const struct rbh_filter *filter,
                    const struct rbh_filter_options *options,
                    struct sqlite_filter_where *where)
{
    if (!filter2sql(filter, where, 0, false))
        return NULL;

    return true;
}

static bool
bind_filter_values(struct sqlite_cursor *cursor,
                   const struct rbh_filter *filter);

static bool
bind_value(struct sqlite_cursor *cursor, const struct rbh_value *value,
           bool bin_as_string);

static bool
bind_sequence(struct sqlite_cursor *cursor, const struct rbh_value *values,
              size_t count)
{
    for (size_t i = 0; i < count; i++)
        if (!bind_value(cursor, &values[i], false))
            return false;

    return true;
}

static bool
bind_map(struct sqlite_cursor *cursor, const struct rbh_value_map *map)
{
    /* Not sure what this would mean since this is not used for now */
    return false;
}

static const char *
sqlite_regex(const struct rbh_value *regex)
{
    const char *value;
    char *tmp;

    if (regex->regex.options & RBH_RO_SHELL_PATTERN)
        value = shell2pcre(regex->regex.string);
    else
        value = regex->regex.string;

    if (regex->regex.options & RBH_RO_CASE_INSENSITIVE) {
        asprintf(&tmp, "(?i)%s", value);
        value = tmp;
    }

    return value;
}

static bool
bind_value(struct sqlite_cursor *cursor, const struct rbh_value *value,
           bool bin_as_string)
{
    char *buf;

    switch (value->type) {
    case RBH_VT_BOOLEAN:
        return sqlite_cursor_bind_int64(cursor, value->boolean ? 1 : 0);
    case RBH_VT_INT32:
        return sqlite_cursor_bind_int64(cursor, value->int32);
    case RBH_VT_UINT32:
        return sqlite_cursor_bind_int64(cursor, value->uint32);
    case RBH_VT_INT64:
        return sqlite_cursor_bind_int64(cursor, value->int64);
    case RBH_VT_UINT64:
        if (value->uint64 >= INT64_MAX)
            return sqlite_cursor_bind_int64(cursor, INT64_MAX);

        return sqlite_cursor_bind_int64(cursor, value->uint64);
    case RBH_VT_STRING:
        return sqlite_cursor_bind_string(cursor, value->string);
    case RBH_VT_BINARY:
        if (value->binary.size == 0)
            return true;

        if (bin_as_string) {
            buf = malloc(2 * value->binary.size + 1);
            if (!buf)
                return false;

            bin2hex(value, buf);
            return sqlite_cursor_bind_string(cursor, buf);
        }

        return sqlite_cursor_bind_binary(cursor, value->binary.data,
                                         value->binary.size);
    case RBH_VT_REGEX:
        return sqlite_cursor_bind_string(cursor, sqlite_regex(value));
    case RBH_VT_SEQUENCE:
        return bind_sequence(cursor, value->sequence.values,
                             value->sequence.count);
    case RBH_VT_MAP:
        return bind_map(cursor, &value->map);
    }
    __builtin_unreachable();
}

static bool
bind_comparison_values(struct sqlite_cursor *cursor,
                       const struct rbh_filter *filter)
{
    char *xattr;
    int rc;

    switch (filter->op) {
    case RBH_FOP_REGEX:
        if (filter->compare.field.fsentry == RBH_FP_INODE_XATTRS ||
            filter->compare.field.fsentry == RBH_FP_NAMESPACE_XATTRS) {

            rc = asprintf(&xattr, "$.%s", filter->compare.field.xattr);
            if (rc == -1)
                return false;

            return sqlite_cursor_bind_string(cursor, xattr) &&
                bind_value(cursor, &filter->compare.value, true);
        }
    case RBH_FOP_BITS_ANY_SET:
    case RBH_FOP_BITS_ALL_SET:
    case RBH_FOP_BITS_ANY_CLEAR:
    case RBH_FOP_BITS_ALL_CLEAR:
        switch (filter->compare.field.fsentry) {
        case RBH_FP_NAME:
        case RBH_FP_SYMLINK:
        case RBH_FP_STATX:
            return bind_value(cursor, &filter->compare.value, false);
        default:
            break;
        }

        rc = asprintf(&xattr, "$.%s", filter->compare.field.xattr);
        if (rc == -1)
            return false;

        return sqlite_cursor_bind_string(cursor, xattr) &&
            bind_value(cursor, &filter->compare.value, true);
    default:
        break;
    }

    if (filter->op != RBH_FOP_IN &&
        (filter->compare.field.fsentry == RBH_FP_INODE_XATTRS ||
         filter->compare.field.fsentry == RBH_FP_NAMESPACE_XATTRS)) {
        rc = asprintf(&xattr, "$.%s", filter->compare.field.xattr);
        if (rc == -1)
            return false;

        if (filter->op == RBH_FOP_EXISTS)
            /* the binary value is already managed when creating the query */
            return sqlite_cursor_bind_string(cursor, xattr);

        return sqlite_cursor_bind_string(cursor, xattr) &&
            bind_value(cursor, &filter->compare.value, true);
    }

    return bind_value(cursor, &filter->compare.value, false);
}

static bool
bind_logical_values(struct sqlite_cursor *cursor,
                    const struct rbh_filter *filter)
{
    for (size_t i = 0; i < filter->logical.count; i++) {
        if (!filter->logical.filters[i])
            continue;

        if (!bind_filter_values(cursor, filter->logical.filters[i]))
            return false;
    }

    return true;
}

static bool
bind_array_values(struct sqlite_cursor *cursor, const struct rbh_filter *filter)
{
    char *xattr;
    int rc;

    rc = asprintf(&xattr, "$.%s", filter->array.field.xattr);
    if (rc == -1)
        return false;

    if (!sqlite_cursor_bind_string(cursor, xattr))
        return false;

    for (size_t i = 0; i < filter->array.count; i++) {
        const struct rbh_filter * subfilter = filter->array.filters[i];

        if (!bind_value(cursor, &subfilter->compare.value, true))
            return false;
    }

    return true;
}

static bool
bind_filter_values(struct sqlite_cursor *cursor,
                   const struct rbh_filter *filter)
{
    if (rbh_is_comparison_operator(filter->op))
        return bind_comparison_values(cursor, filter);
    else if (rbh_is_get_operator(filter->op))
        return bind_filter_values(cursor, filter->get.filter);
    else if (rbh_is_logical_operator(filter->op))
        return bind_logical_values(cursor, filter);
    else if (rbh_is_array_operator(filter->op))
        return bind_array_values(cursor, filter);
    else
        abort();
}

static bool
sqlite_statement_from_filter(struct sqlite_iterator *iter,
                             const struct rbh_filter *filter,
                             const struct rbh_filter_options *options)
{
    char *query =
        "select entries.id, parent_id, name, "
        "mask, blksize, nlink, uid, gid, "
        "mode, type, ino, size, blocks, attributes, "
        "atime_sec, atime_nsec, "
        "btime_sec, btime_nsec, "
        "ctime_sec, ctime_nsec, "
        "mtime_sec, mtime_nsec, "
        "rdev_major, rdev_minor, "
        "dev_major, dev_minor, mnt_id, "
        "entries.xattrs, ns.xattrs, symlink "
        "from entries join ns on entries.id = ns.id";
    struct sqlite_filter_where where = {0};
    char *full_query = query;
    int save_errno;

    if (filter) {
        int rc;

        if (!filter2where_clause(filter, options, &where))
            return false;

        if (where.clause_len > 0) {
            rc = asprintf(&full_query, "%s where %s", query, where.clause);
            if (rc == -1) {
                errno = ENOMEM;
                return false;
            }
        }
    }

    if (!sqlite_setup_query(&iter->cursor, full_query))
        goto free_query;

    if (where.clause_len > 0) {
        if (!bind_filter_values(&iter->cursor, filter))
            goto free_query;

        free(full_query);
    }

    debug("query: %s", sqlite3_expanded_sql(iter->cursor.stmt));
    return true;

free_query:
    if (query != full_query) {
        save_errno = errno;
        free(full_query);
        errno = save_errno;
    }

    return false;
}

struct rbh_mut_iterator *
sqlite_backend_filter(void *backend, const struct rbh_filter *filter,
                      const struct rbh_filter_options *options,
                      const struct rbh_filter_output *output)
{
    struct sqlite_iterator *iter = sqlite_iterator_new();
    struct sqlite_backend *sqlite = backend;

    if (!iter)
        return NULL;

    sqlite_cursor_setup(sqlite, &iter->cursor);
    if (!sqlite_statement_from_filter(iter, filter, options)) {
        int save_errno = errno;
        sqlite_iter_destroy(iter);
        errno = save_errno;
        return NULL;
    }

    return &iter->iter;
}

static const struct rbh_filter ROOT_FILTER = {
    .op = RBH_FOP_EQUAL,
    .compare = {
        .field = {
            .fsentry = RBH_FP_PARENT_ID,
        },
        .value = {
            .type = RBH_VT_BINARY,
            .binary = {
                .size = 0,
            },
        },
    },
};

struct rbh_fsentry *
sqlite_backend_root(void *backend,
                    const struct rbh_filter_projection *projection)
{
    return rbh_backend_filter_one(backend, &ROOT_FILTER, projection);
}
