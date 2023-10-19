#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <inttypes.h>
#include <error.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <jansson.h>

#include "robinhood/backends/hestia.h"
#include "robinhood/sstack.h"
#include "robinhood/statx.h"

#include "enricher.h"
#include "internals.h"

#include "hestia/hestia.h"

static __thread bool hestia_is_init = false;

static void __attribute__((destructor))
exit_hestia(void)
{
    if (hestia_is_init)
        hestia_finish();
}

int
str2int64_t(const char *input, int64_t *result)
{
    char *end;

    errno = 0;
    *result = strtoll(input, &end, 0);
    if (errno) {
        return -1;
    } else if ((!*result && input == end) || *end != '\0') {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

static void
fill_last_modified_time(json_t *value, struct rbh_statx *statx)
{
    const char *str_val = json_string_value(value);
    int64_t val;

    if (str2int64_t(str_val, &val))
        error(EXIT_FAILURE, errno, "'%s' is not a valid timestamp", str_val);

    statx->stx_mtime.tv_sec = val;
    statx->stx_mtime.tv_nsec = 0;
    statx->stx_mask |= RBH_STATX_MTIME;
}

static void
fill_creation_time(json_t *value, struct rbh_statx *statx)
{
    const char *str_val = json_string_value(value);
    int64_t val;

    if (str2int64_t(str_val, &val))
        error(EXIT_FAILURE, errno, "'%s' is not a valid timestamp", str_val);

    statx->stx_btime.tv_sec = val;
    statx->stx_btime.tv_nsec = 0;
    statx->stx_mask |= RBH_STATX_BTIME;
}

static void
fill_size(json_t *value, struct rbh_statx *statx)
{
    const char *str_val = json_string_value(value);
    int64_t val;

    if (str2int64_t(str_val, &val))
        error(EXIT_FAILURE, errno, "'%s' is not a valid size", str_val);

    statx->stx_size = (uint64_t) val;
    statx->stx_mask |= RBH_STATX_SIZE;
}

static struct rbh_value *
fill_attribute(json_t *value)
{
    struct rbh_value *result_value;
    int type = json_typeof(value);

    result_value = malloc(sizeof(*result_value));
    if (result_value == NULL)
        error(EXIT_FAILURE, errno, "malloc in fill_attribute");

    switch (type) {
    case JSON_STRING:
        result_value->type = RBH_VT_STRING;
        result_value->string = strdup(json_string_value(value));
        if (result_value->string == NULL)
            error(EXIT_FAILURE, errno, "strdup in fill_attribute");

        break;
    default:
        result_value->type = RBH_VT_STRING;
        result_value->string = "null";
    }

    return result_value;
}

static void
fill_attributes(json_t *attrs, struct rbh_statx *statx,
                struct rbh_value_pair *pairs, size_t *count)
{
    const char *key;
    json_t *value;
    int index = 0;

    *count = json_object_size(attrs);
    if (*count == 0) {
        pairs = NULL;
        return;
    }

    *count -= 3;

    json_object_foreach(attrs, key, value) {
        if (key[0] == 'l' && !strcmp(&key[1], "ast_modified_time")) {
            fill_last_modified_time(value, statx);
            continue;
        } else if (key[0] == 'c' && !strcmp(&key[1], "reation_time")) {
            fill_creation_time(value, statx);
            continue;
        } else if (key[0] == 's' && !strcmp(&key[1], "ize")) {
            fill_size(value, statx);
            continue;
        }

        pairs[index].key = strdup(key);
        pairs[index].value = fill_attribute(value);
        index++;
    }
}

static int
hestia_enrich(struct rbh_fsevent *enriched, struct rbh_value_pair *pairs)
{
    char *attrs_buffer = NULL;
    struct rbh_statx *statx;
    json_t *attrs = NULL;
    int total_count = 0;
    int attrs_len = 0;
    size_t nb_pairs;
    int rc;

    statx = malloc(sizeof(*statx));
    if (statx == NULL)
        return -1;

    statx->stx_mask = 0;

    /* XXX: Missing check that we want to retrieve Hestia attributes, will be
     * added later
     */
    rc = hestia_read(HESTIA_OBJECT, HESTIA_QUERY_IDS, HESTIA_ID, 0, 0,
                     enriched->id.data, enriched->id.size, HESTIA_IO_JSON,
                     &attrs_buffer, &attrs_len, &total_count);
    if (rc != 0) {
        fprintf(stderr, "Failed to get Hestia attributes ('%d').\n", rc);
        return rc;
    }

    attrs = json_loads(attrs_buffer, 0, NULL);
    if (attrs == NULL)
        goto free_output;

    printf("Attributes of '%s': %s\n",
           enriched->id.data, json_dumps(attrs, JSON_INDENT(4)));

    fill_attributes(attrs, statx, pairs, &nb_pairs);

    enriched->upsert.statx = statx;
    enriched->xattrs.count += nb_pairs;

    json_decref(attrs);

free_output:
    hestia_free_output(&attrs_buffer);

    return 0;
}

static int
enrich(struct enricher *enricher, const struct rbh_fsevent *original)
{
    struct rbh_fsevent *enriched = &enricher->fsevent;
    struct rbh_value_pair *pairs = enricher->pairs;
    size_t pair_count = enricher->pair_count;

    *enriched = *original;
    enriched->xattrs.count = 0;

    for (size_t i = 0; i < original->xattrs.count; i++) {
        const struct rbh_value_pair *pair = &original->xattrs.pairs[i];
        const struct rbh_value_map *partials;

        if (strcmp(pair->key, "rbh-fsevents")) {
            if (enriched->xattrs.count + 1 >= pair_count) {
                void *tmp;

                tmp = reallocarray(pairs, pair_count << 1, sizeof(*pairs));
                if (tmp == NULL)
                    return -1;
                enricher->pairs = pairs = tmp;
                enricher->pair_count = pair_count = pair_count << 1;
            }
            pairs[enriched->xattrs.count++] = original->xattrs.pairs[i];
            continue;
        }

        if (pair->value == NULL || pair->value->type != RBH_VT_MAP) {
            errno = EINVAL;
            return -1;
        }
        partials = &pair->value->map;

        for (size_t i = 0; i < partials->count; i++) {
            int rc;

            rc = hestia_enrich(enriched, &pairs[enriched->xattrs.count]);
            if (rc == -1)
                return -1;
        }

    }
    enriched->xattrs.pairs = pairs;

    return 0;
}

static const void *
hestia_enricher_iter_next(void *iterator)
{
    struct enricher *enricher = iterator;
    const void *fsevent;

    fsevent = rbh_iter_next(enricher->fsevents);
    if (fsevent == NULL)
        return NULL;

    if (enrich(enricher, fsevent))
        return NULL;

    return &enricher->fsevent;
}

void
hestia_enricher_iter_destroy(void *iterator)
{
    struct enricher *enricher = iterator;

    rbh_iter_destroy(enricher->fsevents);
    free(enricher->pairs);
    free(enricher);
}

static const struct rbh_iterator_operations HESTIA_ENRICHER_ITER_OPS = {
    .next = hestia_enricher_iter_next,
    .destroy = hestia_enricher_iter_destroy,
};

static const struct rbh_iterator HESTIA_ENRICHER_ITERATOR = {
    .ops = &HESTIA_ENRICHER_ITER_OPS,
};

#define INITIAL_PAIR_COUNT (1 << 7)

struct rbh_iterator *
hestia_iter_enrich(struct rbh_iterator *fsevents)
{
    struct rbh_value_pair *pairs;
    struct enricher *enricher;

    pairs = reallocarray(NULL, INITIAL_PAIR_COUNT, sizeof(*pairs));
    if (pairs == NULL)
        return NULL;

    enricher = malloc(sizeof(*enricher));
    if (enricher == NULL) {
        int save_errno = errno;

        free(pairs);
        errno = save_errno;
        return NULL;
    }

    enricher->iterator = HESTIA_ENRICHER_ITERATOR;
    enricher->backend = NULL;
    enricher->fsevents = fsevents;
    enricher->mount_fd = -1;
    enricher->mount_path = NULL;
    enricher->pairs = pairs;
    enricher->pair_count = INITIAL_PAIR_COUNT;
    enricher->symlink = NULL;

    return &enricher->iterator;
}

/*----------------------------------------------------------------------------*
 *                           hestia_backend_enrich                            *
 *----------------------------------------------------------------------------*/

static struct rbh_iterator *
hestia_build_enrich_iter(void *_builder, struct rbh_iterator *fsevents)
{
    (void) _builder;

    return hestia_iter_enrich(fsevents);
}

void
hestia_iter_builder_destroy(void *_builder)
{
    struct enrich_iter_builder *builder = _builder;

    free(builder->backend);
    free(builder);
}

static const struct enrich_iter_builder_operations
HESTIA_ENRICH_ITER_BUILDER_OPS = {
    .build_iter = hestia_build_enrich_iter,
    .destroy = hestia_iter_builder_destroy,
};

const struct enrich_iter_builder HESTIA_ENRICH_ITER_BUILDER = {
    .name = "hestia",
    .ops = &HESTIA_ENRICH_ITER_BUILDER_OPS,
};

struct enrich_iter_builder *
hestia_enrich_iter_builder(struct rbh_backend *backend)
{
    struct enrich_iter_builder *builder;

    builder = malloc(sizeof(*builder));
    if (builder == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    *builder = HESTIA_ENRICH_ITER_BUILDER;

    builder->backend = backend;
    builder->mount_fd = -1;
    builder->mount_path = NULL;

    /* XXX: not specifying the configuration file provokes a segfault
     * when calling hestia_finish
     */
    //hestia_initialize(NULL, NULL, NULL);
    hestia_initialize("/etc/hestia/hestiad.yaml", NULL, NULL);
    hestia_is_init = true;

    return builder;
}
