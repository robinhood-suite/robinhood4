#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <jansson.h>

#include "robinhood/backends/hestia.h"
#include "robinhood/sstack.h"
#include "robinhood/statx.h"

#include "hestia_glue.h"

/*----------------------------------------------------------------------------*
 |                              hestia_iterator                               |
 *----------------------------------------------------------------------------*/

struct hestia_iterator {
    struct rbh_mut_iterator iterator;
    struct rbh_sstack *values;
    struct hestia_id *ids;
    size_t current_id; /* index of the object in 'ids' that will be managed in
                        * the next call to "hestia_iter_next". */
    size_t length;
};

static void
fill_creation_time(json_t *value, struct rbh_statx *statx)
{
    json_int_t val = json_integer_value(value);

    statx->stx_btime.tv_sec = val;
    statx->stx_btime.tv_nsec = 0;
    statx->stx_mask |= RBH_STATX_BTIME;
}

static void
fill_last_modified(json_t *value, struct rbh_statx *statx)
{
    json_int_t val = json_integer_value(value);

    statx->stx_mtime.tv_sec = val;
    statx->stx_mtime.tv_nsec = 0;
    statx->stx_mask |= RBH_STATX_MTIME;
}

static void
fill_tier(json_t *value, struct rbh_value *tier_value)
{
    json_int_t val = json_integer_value(value);

    tier_value->type = RBH_VT_UINT64;
    tier_value->uint64 = val;
}

static int
parse_attrs(const char *obj_attrs, struct rbh_statx *statx,
            struct rbh_value_pair **_pair, struct rbh_sstack *sstack)
{
    struct rbh_value_pair *pairs;
    struct rbh_value *values;
    json_t *attrs = NULL;
    size_t nb_attrs;
    const char *key;
    json_t *value;
    int idx = 0;
    int rc = 0;

    attrs = json_loads(obj_attrs, 0, NULL);
    if (attrs == NULL)
        return -1;

    nb_attrs = json_object_size(attrs);
    if (nb_attrs == 0)
        goto err;

    /* 2 attributes will be filed in the statx structure, so they are not to be
     * filled as rbh_values.
     */
    values = rbh_sstack_push(sstack, NULL, sizeof(*values) * (nb_attrs - 2));
    if (values == NULL)
        goto err;

    pairs = rbh_sstack_push(sstack, NULL, sizeof(*pairs) * (nb_attrs - 2));
    if (pairs == NULL)
        goto err;

    json_object_foreach(attrs, key, value) {
        switch (key[0]) {
        case 'c':
            if (!strcmp(&key[1], "reation_time")) {
                fill_creation_time(value, statx);
                continue;
            }
            break;
        case 'l':
            if (!strcmp(&key[1], "ast_modified")) {
                fill_last_modified(value, statx);
                continue;
            }
            break;
        case 't':
            if (!strcmp(&key[1], "ier")) {
                rc = fill_tier(value, &values[idx]);
                if (rc)
                    goto err;
            }
            break;
        default:
            continue;
        }

        pairs[idx].key = key;
        pairs[idx].value = &values[idx];
        idx++;
    }

    *_pair = pairs;
    rc = idx;

err:
    json_decref(attrs);

    return rc;
}

static int
fill_path(char *path, struct rbh_value_pair **_pairs, struct rbh_sstack *values)
{
    struct rbh_value *path_value;
    struct rbh_value_pair *pair;

    path_value = rbh_sstack_push(values, NULL, sizeof(*path_value));
    if (path_value == NULL)
        return -1;

    path_value->type = RBH_VT_STRING;
    path_value->string = path;

    pair = rbh_sstack_push(values, NULL, sizeof(*pair));
    if (pair == NULL)
        return -1;

    pair->key = "path";
    pair->value = path_value;

    *_pairs = pair;

    return 0;
}

static void *
hestia_iter_next(void *iterator)
{
    struct hestia_iterator *hestia_iter = iterator;
    struct rbh_statx statx = { .stx_mask = 0 };
    struct rbh_fsentry *fsentry = NULL;
    struct rbh_value_pair *inode_pairs;
    struct rbh_value_map inode_xattrs;
    struct rbh_value_pair *ns_pairs;
    struct rbh_value_map ns_xattrs;
    struct rbh_id parent_id;
    char *obj_attrs = NULL;
    struct hestia_id *obj;
    char *name = NULL;
    struct rbh_id id;
    size_t attrs_len;
    int rc;

    if (hestia_iter->current_id >= hestia_iter->length) {
        errno = ENODATA;
        return NULL;
    }

    obj = &hestia_iter->ids[hestia_iter->current_id];

    rc = list_object_attrs(obj, &obj_attrs, &attrs_len);
    if (rc)
        return NULL;

    /* Use the hestia_id of each file as rbh_id */
    id.data = rbh_sstack_push(hestia_iter->values, obj, sizeof(*obj));
    if (id.data == NULL)
        goto err;

    id.size = sizeof(*obj);

    /* All objects have no parent */
    parent_id.size = 0;

    rc = asprintf(&name, "%ld-%ld", obj->higher, obj->lower);
    if (rc <= 0)
        goto err;

    rc = parse_attrs(obj_attrs, &statx, &inode_pairs, hestia_iter->values);
    if (rc <= 0)
        goto err;

    inode_xattrs.pairs = inode_pairs;
    inode_xattrs.count = rc;

    rc = fill_path(name, &ns_pairs, hestia_iter->values);
    if (rc)
        goto err;

    ns_xattrs.pairs = ns_pairs;
    ns_xattrs.count = 1;

    fsentry = rbh_fsentry_new(&id, &parent_id, name, &statx, &ns_xattrs,
                              &inode_xattrs, NULL);
    if (fsentry == NULL)
        goto err;

    hestia_iter->current_id++;

err:
    free(name);
    free(obj_attrs);

    return fsentry;
}

static void
hestia_iter_destroy(void *iterator)
{
    struct hestia_iterator *hestia_iter = iterator;

    rbh_sstack_destroy(hestia_iter->values);
    free(hestia_iter->ids);
    free(hestia_iter);
}

static const struct rbh_mut_iterator_operations HESTIA_ITER_OPS = {
    .next = hestia_iter_next,
    .destroy = hestia_iter_destroy,
};

static const struct rbh_mut_iterator HESTIA_ITER = {
    .ops = &HESTIA_ITER_OPS,
};

struct hestia_iterator *
hestia_iterator_new()
{
    struct hestia_iterator *hestia_iter = NULL;
    struct hestia_id *ids = NULL;
    uint8_t *tiers = NULL;
    size_t tiers_len;
    size_t ids_len;
    int save_errno;
    int rc;

    hestia_iter = malloc(sizeof(*hestia_iter));
    if (hestia_iter == NULL)
        return NULL;

    rc = list_tiers(&tiers, &tiers_len);
    if (rc)
        goto err;

    rc = list_objects(tiers, tiers_len, &ids, &ids_len);
    free(tiers);
    if (rc)
        goto err;

    hestia_iter->iterator = HESTIA_ITER;
    hestia_iter->ids = ids;
    hestia_iter->length = ids_len;
    hestia_iter->current_id = 0;

    hestia_iter->values = rbh_sstack_new(1 << 10);
    if (hestia_iter->values == NULL)
        goto err;

    return hestia_iter;

err:
    save_errno = errno;
    free(ids);
    free(hestia_iter);
    errno = save_errno;
    return NULL;
}

/*----------------------------------------------------------------------------*
 |                               hestia_backend                               |
 *----------------------------------------------------------------------------*/

struct hestia_backend {
    struct rbh_backend backend;
    struct hestia_iterator *(*iter_new)();
};

    /*--------------------------------------------------------------------*
     |                              filter()                              |
     *--------------------------------------------------------------------*/

static struct rbh_mut_iterator *
hestia_backend_filter(void *backend, const struct rbh_filter *filter,
                      const struct rbh_filter_options *options)
{
    struct hestia_backend *hestia = backend;
    struct hestia_iterator *hestia_iter;

    if (filter != NULL) {
        errno = ENOTSUP;
        return NULL;
    }

    if (options->skip > 0 || options->limit > 0 || options->sort.count > 0) {
        errno = ENOTSUP;
        return NULL;
    }

    hestia_iter = hestia->iter_new();
    if (hestia_iter == NULL)
        return NULL;

    return &hestia_iter->iterator;
}

    /*--------------------------------------------------------------------*
     |                             destroy()                              |
     *--------------------------------------------------------------------*/

static void
hestia_backend_destroy(void *backend)
{
    struct hestia_backend *hestia = backend;

    free(hestia);
}

static const struct rbh_backend_operations HESTIA_BACKEND_OPS = {
    .filter = hestia_backend_filter,
    .destroy = hestia_backend_destroy,
};

static const struct rbh_backend HESTIA_BACKEND = {
    .id = RBH_BI_HESTIA,
    .name = RBH_HESTIA_BACKEND_NAME,
    .ops = &HESTIA_BACKEND_OPS,
};

struct rbh_backend *
rbh_hestia_backend_new(__attribute__((unused)) const char *path)
{
    struct hestia_backend *hestia;

    hestia = malloc(sizeof(*hestia));
    if (hestia == NULL)
        return NULL;

    hestia->iter_new = hestia_iterator_new;
    hestia->backend = HESTIA_BACKEND;

    return &hestia->backend;
}
