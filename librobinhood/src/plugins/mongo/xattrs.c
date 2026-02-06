/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <error.h>

#include "robinhood/value.h"
#include "robinhood/utils.h"

#include "mongo.h"
#include "utils.h"

/*----------------------------------------------------------------------------*
 |                          bson_append_setxattrs()                           |
 *----------------------------------------------------------------------------*/

#define ONSTACK_KEYLEN_MAX 128

static bool
bson_append_xattr(bson_t *bson, const char *prefix, const char *xattr,
                  const struct rbh_value *value)
{
    char onstack[ONSTACK_KEYLEN_MAX];
    char *key = onstack;
    bool success;
    int keylen;

    keylen = snprintf(key, sizeof(onstack), "%s.%s", prefix, xattr);
    if (keylen < 0)
        return false;
    if ((size_t)keylen >= sizeof(onstack))
        keylen = asprintf(&key, "%s.%s", prefix, xattr);
    if (keylen < 0)
        return false;

    if (value == NULL)
        success = bson_append_null(bson, key, keylen);
    else
        success = bson_append_rbh_value(bson, key, keylen, value);
    if (key != onstack)
        free(key);

    errno = ENOBUFS;
    return success;
}

static bson_t *
make_cond_bson(const struct rbh_value *ts)
{
    assert(ts->type ==  RBH_VT_INT64);

    return BCON_NEW("$or", "[",
                     "{",
                       "$gt",  "[",
                         BCON_INT64(ts->int64),
                         BCON_UTF8("$xattrs.nb_children.timestamp"),
                       "]",
                      "}",
                      "{",
                        "$and", "[",
                          "{",
                            "$eq", "[",
                              BCON_INT64(ts->int64),
                              BCON_UTF8("$xattrs.nb_children.timestamp"),
                            "]",
                           "}",
                           "{",
                             "$eq", "[",
                               BCON_BOOL(false),
                               BCON_UTF8("$xattrs.nb_children.final"),
                             "]",
                           "}",
                        "]",
                      "}",
                    "]");
}

__attribute__((unused))
static bool
bson_append_set_nb_children(bson_t *bson, const char *prefix, const char *xattr,
                            const struct rbh_value *value)
{

    const struct rbh_value *timestamp;
    bson_t doc, cond;
    bool rc = true;
    bson_t *_cond;

    timestamp = rbh_map_find(&value->map, "timestamp");
    if (timestamp == NULL)
        return false;

    _cond = make_cond_bson(timestamp);
    if (_cond == NULL)
        return false;

    if (!(BSON_APPEND_DOCUMENT_BEGIN(bson, "xattrs.nb_children", &doc) &&
         BSON_APPEND_ARRAY_BEGIN(&doc, "$cond", &cond) &&
         BSON_APPEND_DOCUMENT(&cond, "0", _cond) &&
         bson_append_rbh_value(&cond, "1", 1, value) &&
         BSON_APPEND_UTF8(&cond, "2", "$xattrs.nb_children") &&
         bson_append_array_end(&doc, &cond) &&
         bson_append_document_end(bson, &doc)))
        rc = false;

    bson_destroy(_cond);

    return rc;
}

/**
 *  xattrs = { "xattr1" : { "op" : value }, "xattr2": { "op": value }}
 */
bool
bson_append_xattrs(const char *prefix, const struct rbh_value_map *xattrs,
                   bson_t *set, bson_t *unset, bson_t *inc)
{
    for (size_t i = 0; i < xattrs->count; i++) {
        const struct rbh_value_map *op_map = &xattrs->pairs[i].value->map;
        const char *xattr = xattrs->pairs[i].key;
        const struct rbh_value *value;
        const char *op;
        bson_t *bson;

        assert(op_map->count == 1);

        op = op_map->pairs->key;
        value = op_map->pairs->value;

        switch (op[0]) {
        case 's':
            bson = set;
            break;
        case 'u':
            bson = unset;
            break;
        case 'i':
            bson = inc;
            break;
        default:
            return false;
        }

        if (!bson_append_xattr(bson, prefix, xattr,
                               bson == unset ? NULL : value))
            return false;
    }

    return true;
}

enum namespace_token {
    NT_UNKNOWN,
    NT_PARENT,
    NT_NAME,
    NT_XATTRS,
};

static enum namespace_token
namespace_tokenizer(const char *key)
{
    switch (*key++) {
    case 'n': /* name */
        if (strcmp(key, "ame"))
            break;
        return NT_NAME;
    case 'p': /* parent */
        if (strcmp(key, "arent"))
            break;
        return NT_PARENT;
    case 'x': /* xattrs */
        if (strcmp(key, "attrs"))
            break;
        return NT_XATTRS;
    }
    return NT_UNKNOWN;
}

bool
bson_iter_namespace(bson_iter_t *iter, struct rbh_fsentry *fsentry,
                    char **buffer, size_t *bufsize)
{
    size_t size = *bufsize;
    char *data = *buffer;

    while (bson_iter_next(iter)) {
        bson_iter_t subiter;
        bson_iter_t tmp;

        switch (namespace_tokenizer(bson_iter_key(iter))) {
        case NT_UNKNOWN:
            break;
        case NT_PARENT:
            if (!BSON_ITER_HOLDS_NULL(iter) && !BSON_ITER_HOLDS_BINARY(iter))
                goto out_einval;

            if (!bson_iter_rbh_id(iter, &fsentry->parent_id))
                return false;
            fsentry->mask |= RBH_FP_PARENT_ID;
            break;
        case NT_NAME:
            if (!BSON_ITER_HOLDS_UTF8(iter))
                goto out_einval;

            fsentry->name = bson_iter_utf8(iter, NULL);
            fsentry->mask |= RBH_FP_NAME;
            break;
        case NT_XATTRS:
            if (!BSON_ITER_HOLDS_DOCUMENT(iter))
                goto out_einval;
            bson_iter_recurse(iter, &subiter);

            bson_iter_recurse(iter, &tmp);
            if (!bson_iter_rbh_value_map(&subiter, &fsentry->xattrs.ns,
                                         bson_iter_count(&tmp), &data, &size))
                return false;
            fsentry->mask |= RBH_FP_NAMESPACE_XATTRS;
            break;
        }
    }

    *buffer = data;
    *bufsize = size;
    return true;

out_einval:
    errno = EINVAL;
    return false;
}

