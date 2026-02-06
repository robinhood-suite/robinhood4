/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <lustre/lustreapi.h>
#include <linux/lustre/lustre_fid.h>

#include "lustre_utils.h"

#include "source.h"
#include "utils.h"

/* BSON results:
 * { "statx" : { "uid" : x, "gid" : y } }
 */
static void
fill_uidgid(struct changelog_rec *record, struct rbh_statx *statx)
{
    struct changelog_ext_uidgid *uidgid;

    statx->stx_mask |= RBH_STATX_UID | RBH_STATX_GID;
    uidgid = changelog_rec_uidgid(record);
    statx->stx_uid = uidgid->cr_uid;
    statx->stx_gid = uidgid->cr_gid;
}


static struct rbh_value *
build_op_xattr(const char *op, struct rbh_value *xattr_value)
{
    struct rbh_value_pair *pair_op;
    struct rbh_value value;

    pair_op = source_stack_alloc(NULL, sizeof(*pair_op));

    pair_op->key = op;
    pair_op->value = source_stack_alloc(xattr_value, sizeof(*xattr_value));

    value.type = RBH_VT_MAP;
    value.map.count = 1;
    value.map.pairs = pair_op;

    return source_stack_alloc(&value, sizeof(value));
}

/* BSON results:
 * { "xattrs": { "fid" : x } }
 */
struct rbh_value *
fill_xattrs_fid(void *arg)
{
    struct changelog_rec *record = (struct changelog_rec *)arg;
    struct rbh_value lu_fid_value;
    struct rbh_value *value;

    lu_fid_value.type = RBH_VT_BINARY;
    lu_fid_value.binary.size = sizeof(record->cr_tfid);
    lu_fid_value.binary.data = source_stack_alloc(
        (const char *)&record->cr_tfid, sizeof(record->cr_tfid));
    if (lu_fid_value.binary.data == NULL)
        return NULL;

    value = source_stack_alloc(&lu_fid_value, sizeof(lu_fid_value));
    if (value == NULL)
        return NULL;

    return build_op_xattr("set", value);
}

/* BSON results:
 * { "xattrs": { "mdt_index" : x } }
 */
struct rbh_value *
fill_xattrs_mdt_index(void *arg)
{
    int32_t mdt_index = *((int32_t *) arg);
    struct rbh_value mdt_index_value;
    struct rbh_value *value;

    mdt_index_value.type = RBH_VT_INT32;
    mdt_index_value.int32 = mdt_index;

    value = source_stack_alloc(&mdt_index_value, sizeof(mdt_index_value));
    if (value == NULL)
        return NULL;

    return build_op_xattr("set", value);
}

/* BSON results:
 * { "xattrs": { "nb_children" : { value : x [+-]1 } } }
 */
struct rbh_value *
fill_xattrs_nb_children(void *arg)
{
    struct rbh_value nb_children_value;
    int64_t inc = *((int64_t *) arg);
    struct rbh_value *value;

    nb_children_value.type = RBH_VT_INT64;
    nb_children_value.int64 = inc;

    value = source_stack_alloc(&nb_children_value, sizeof(nb_children_value));
    if (value == NULL)
        return NULL;

    return build_op_xattr("inc", value);
}

static struct rbh_value *
build_statx_mask(void *arg)
{
    uint32_t enrich_mask = *(uint32_t *)arg;
    const struct rbh_value MASK = {
        .type = RBH_VT_UINT32,
        .uint32 = enrich_mask,
    };
    struct rbh_value *mask;

    mask = source_stack_alloc(NULL, sizeof(*mask));
    if (mask == NULL)
        return NULL;
    *mask = MASK;

    return mask;
}

static struct rbh_value *
build_xattrs(void *arg)
{
    struct rbh_value *xattr_sequence;
    char *xattr_name = (char *)arg;
    struct rbh_value *xattr_value;

    xattr_value = source_stack_alloc(NULL, sizeof(*xattr_value));
    if (xattr_value == NULL)
        return NULL;

    xattr_value->type = RBH_VT_STRING;
    xattr_value->string = source_stack_alloc(xattr_name,
                                             strlen(xattr_name) + 1);
    if (xattr_value->string == NULL)
        return NULL;

    xattr_sequence = source_stack_alloc(NULL, sizeof(*xattr_sequence));
    if (xattr_sequence == NULL)
        return NULL;

    xattr_sequence->type = RBH_VT_SEQUENCE;
    xattr_sequence->sequence.count = 1;
    xattr_sequence->sequence.values = xattr_value;

    return xattr_sequence;
}

static struct rbh_value *
build_symlink_string(void *arg)
{
    const struct rbh_value SYMLINK = {
        .type = RBH_VT_STRING,
        .string = "symlink",
    };

    (void) arg;

    return source_stack_alloc(&SYMLINK, sizeof(SYMLINK));
}

static struct rbh_value *
_fill_enrich(const char *key, struct rbh_value *(*builder)(void *),
             void *arg)
{
    const struct rbh_value ENRICH = {
        .type = RBH_VT_MAP,
        .map = {
            .count = 1,
            .pairs = build_pair(key, builder, arg),
        },
    };
    struct rbh_value *enrich;

    if (ENRICH.map.pairs == NULL)
        return NULL;

    enrich = source_stack_alloc(NULL, sizeof(*enrich));
    if (enrich == NULL)
        return NULL;
    memcpy(enrich, &ENRICH, sizeof(*enrich));

    return enrich;
}

/* BSON results:
 * { "xattrs" : { "rbh-fsevents" : { "xattrs" : [ a, b, c, ... ] } } }
 */
struct rbh_value *
fill_inode_xattrs(void *arg)
{
    return _fill_enrich("xattrs", build_xattrs, arg);
}

/* BSON results:
 * { "xattrs" : { "rbh-fsevents" : { "symlink" : "symlink" } } }
 */
struct rbh_value *
build_symlink_enrich_map(void *arg)
{
    return _fill_enrich("symlink", build_symlink_string, arg);
}

/* The variadic arguments must be given by pairs -a string key and a rbh value-,
 * and finished by a NULL pointer.
 */
int
build_enrich_xattr_fsevent(struct rbh_value_map *xattrs_map,
                           char *key, struct rbh_value *value, ...)
{
    struct rbh_value_pair *pairs;
    va_list args;
    size_t count;
    size_t i;

    /* Loop to retrieve how many pairs we got, for the allocation.
     * The last argument must be NULL.
     */
    va_start(args, value);
    for (count = 0; va_arg(args, void *) != NULL; ++count);
    va_end(args);

    assert(count % 2 == 0); /* count must be even */
    xattrs_map->count = 1 + count / 2;

    pairs = source_stack_alloc(NULL,
                               xattrs_map->count * sizeof(*xattrs_map->pairs));
    if (pairs == NULL)
        return -1;

    pairs[0].key = key;
    pairs[0].value = value;

    va_start(args, value);
    for (i = 1; i < xattrs_map->count; ++i) {
        pairs[i].key = va_arg(args, char *);
        /* value cannot be NULL here, or it will be catch by the assert above */
        pairs[i].value = va_arg(args, struct rbh_value *);
    }
    va_end(args);

    xattrs_map->pairs = pairs;

    return 0;
}

struct rbh_id *
build_id(const struct lu_fid *fid)
{
    struct rbh_id *tmp_id;
    struct rbh_id *id;
    size_t data_size;
    char *data;
    int rc;

    tmp_id = rbh_id_from_lu_fid(fid);

    data_size = tmp_id->size;
    id = source_stack_alloc(NULL, sizeof(*id) + data_size);
    if (id == NULL)
        return NULL;

    data = (char *)(id + 1);
    rc = rbh_id_copy(id, tmp_id, &data, &data_size);
    if (rc)
        return NULL;

    free(tmp_id);

    return id;
}

int
create_statx_uid_gid(struct changelog_rec *record, struct rbh_statx **rec_statx)
{
    *rec_statx = source_stack_alloc(NULL, sizeof(**rec_statx));
    if (*rec_statx == NULL)
        return -1;

    (*rec_statx)->stx_mask = 0;
    (*rec_statx)->stx_mode = 0;
    fill_uidgid(record, *rec_statx);

    return 0;
}

struct rbh_value *
build_statx_map(void *enrich_mask)
{
    struct rbh_value_pair *enrich_values;
    struct rbh_value_pair *statx_pair;
    struct rbh_value_map map;
    struct rbh_value enrich;

    enrich_values = source_stack_alloc(NULL, sizeof(*enrich_values));

    statx_pair = build_pair("statx", build_statx_mask, enrich_mask);

    memcpy(enrich_values, statx_pair, sizeof(*statx_pair));

    map.pairs = enrich_values;
    map.count = 1;

    enrich.map = map;
    enrich.type = RBH_VT_MAP;

    return source_stack_alloc(&enrich, sizeof(enrich));
}

static const char *
get_event_name(unsigned int cl_event)
{
    static const char * const event_name[] = {
        "archive", "restore", "cancel", "release", "remove", "state",
    };

    if (cl_event >= (sizeof(event_name) / sizeof(event_name[0])))
        return "unknown";
    else
        return event_name[cl_event];
}

/* Dump a single record. */
void
dump_changelog(struct lustre_changelog_iterator *records,
               struct changelog_rec *record)
{
    char record_str[RBH_PATH_MAX] = "";
    int left = sizeof(record_str);
    char flag_buff[256] = "";
    char *curr = record_str;
    int len = 0;

    if (record->cr_type == CL_HSM)
        sprintf(flag_buff, "(%s%s,rc=%d)",
                get_event_name(hsm_get_cl_event(record->cr_flags)),
                hsm_get_cl_flags(record->cr_flags) & CLF_HSM_DIRTY ?
                    ",dirty" : "",
                hsm_get_cl_error(record->cr_flags));

    len = snprintf(curr, left, CL_BASE_FORMAT,
                   CL_BASE_ARG(records->mdt_name, record));
    curr += len;
    left -= len;

    if (left > 0 && record->cr_namelen) {
        /* this record has a 'name' field. */
        len = snprintf(curr, left, " " CL_NAME_FORMAT, CL_NAME_ARG(record));
        curr += len;
        left -= len;
    }

    if (left <= 0) {
        record_str[RBH_PATH_MAX - 1] = '\0';
        goto dump;
    }

    if (record->cr_flags & CLF_RENAME) {
        struct changelog_ext_rename *cr_rename;

        cr_rename = changelog_rec_rename(record);
        if (fid_is_sane(&cr_rename->cr_sfid)) {
            len = snprintf(curr, left, " " CL_EXT_FORMAT,
                           PFID(&cr_rename->cr_sfid),
                           PFID(&cr_rename->cr_spfid),
                           (int)changelog_rec_snamelen(record),
                           changelog_rec_sname(record));
            curr += len;
            left -= len;
        }
    }

    if (record->cr_flags & CLF_JOBID) {
        struct changelog_ext_jobid *jobid;

        jobid = changelog_rec_jobid(record);
        if (jobid->cr_jobid[0] != '\0') {
            len = snprintf(curr, left, " J=%s", jobid->cr_jobid);
            curr += len;
            left -= len;
        }
    }

    if (left <= 0)
        record_str[RBH_PATH_MAX - 1] = '\0';

dump:
    fprintf(records->dump_file, "%s\n", record_str);
}
