/* This file is part of RobinHood
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <error.h>
#include <linux/limits.h>

#include "robinhood/value.h"
#include "robinhood/utils.h"

#include "mongo.h"
#include "utils.h"

/*----------------------------------------------------------------------------*
 |                          bson_append_setxattrs()                           |
 *----------------------------------------------------------------------------*/

#define XATTR_ONSTACK_LENGTH 128

static bool
bson_append_xattr(bson_t *bson, const char *prefix, const char *xattr,
                  const struct rbh_value *value)
{
    char onstack[XATTR_ONSTACK_LENGTH];
    int chars_to_escape = 0;
    char *key = onstack;
    bool success;
    int keylen;

    /**
     * Mongo considers the dollar sign as a special characters in keys:
     * - If it is at the start of a string ("$<key>"), it is considered as an
     * Mongo operator.
     * - If it is after a dot and before a string (".$<key>"), it is considered
     * as part of the key.
     * - If it is between dots (".$."), it is considered as a way access all
     * members of the array before the first dot. More specifically, for us, it
     * is how we access all the members of the namespace array for an entry.
     * - Otherwise it's considered a part of a key.
     *
     * The last two cases won't trigger any error. The first is not supposed to
     * happen or that means we did something wrong in the code. The second case
     * however can occur if an entry has an extended attribute with a key of
     * that form, and Mongo will consider it as an error.
     *
     * To handle this case, we must escape the dollar character, but only if it
     * is after a dot, which makes the condition a bit convoluted. Moreover,
     * we do this escaping sequence on the 'xattr' part. This does not conflict
     * with our use of the third case, as that use case is part of the 'prefix',
     * so no issue there.
     *
     * XXX: We use the '\' character for escaping. However, that character is
     * also escaped by Mongo itself, so what is stored in the database is
     * '\\$<rest_of_key>'. It's ugly but at least it avoid issues. In any case,
     * it's unlikely the dollar sign is used as part of a xattr key. And it's
     * even more unlikely that any conflict arises because of another key
     * already set with that same name.
     */
    for (int i = 0; i < strlen(xattr); ++i)
        if ((xattr[i - 1] == '.' && xattr[i] == '$'))
            chars_to_escape++;

    keylen = snprintf(key, sizeof(onstack), "%s.%s", prefix, xattr);
    if (keylen < 0)
        return false;
    if ((size_t) keylen + chars_to_escape >= sizeof(onstack)) {
        key = xmalloc(keylen + chars_to_escape + 1);
        keylen = sprintf(key, "%s.%s", prefix, xattr);
    }
    if (keylen < 0)
        return false;

    if (chars_to_escape) {
        for (int i = strlen(prefix) + 1; chars_to_escape > 0; ++i) {
            if (key[i - 1] == '.' && key[i] == '$') { //&& key[i + 1] != '.') {
                memmove(key + i + 1, key + i, keylen - i + 1);

                key[i] = '\\';
                i++;
                keylen++;
                chars_to_escape--;
            }
        }
    }

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
 *
 *  Only one exception with nb_children:
 *
 *  xattrs = { "nb_children" :
 *              { "op" :
 *                  { "value": 1, "timestamp": xxx, "final": true }
 *              }
 *           }
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

        /* Specific function for the nb_children xattr with a $set operation
         * because of the MPI's backends
         */
        if (strcmp(xattr, "nb_children") == 0 && bson == set) {
            if (!bson_append_set_nb_children(bson, prefix, xattr,
                                             op_map->pairs->value))
                return false;
        } else {
            if (!bson_append_xattr(bson, prefix, xattr,
                                   bson == unset ? NULL : value))
                return false;
        }
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

