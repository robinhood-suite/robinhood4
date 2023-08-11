#include "utils.h"
#include "check-compat.h"
#include "check_macros.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <robinhood/iterator.h>
#include <robinhood/itertools.h>

#include <lustre/lustre_user.h>

static const void *
empty_source_next(void *iterator __attribute__((unused)))
{
    errno = ENODATA;
    return NULL;
}

static void
nop(void *data __attribute__((unused)))
{
}

static const struct rbh_iterator_operations EMPTY_SOURCE_OPS = {
    .next = empty_source_next,
    .destroy = nop,
};

static struct source EMPTY_SOURCE = {
    .name = "test-empty",
    .fsevents = {
        .ops = &EMPTY_SOURCE_OPS,
    },
};

struct source *
empty_source(void)
{
    return &EMPTY_SOURCE;
}

struct event_list_source {
    struct source source;
    struct rbh_iterator *list;
};

static const void *
event_list_next(void *iterator)
{
    struct event_list_source *source = iterator;

    return rbh_iter_next(source->list);
}

static void
event_list_destroy(void *iterator)
{
    struct event_list_source *source = iterator;

    rbh_iter_destroy(source->list);
    free(source);
}

static struct rbh_iterator_operations EVENT_LIST_OPS = {
    .next = event_list_next,
    .destroy = event_list_destroy,
};

static struct source EVENT_LIST_SOURCE = {
    .name = "test-event-list",
    .fsevents = {
        .ops = &EVENT_LIST_OPS,
    },
};

struct source *
event_list_source(struct rbh_fsevent *events, size_t count)
{
    struct event_list_source *source;

    source = malloc(sizeof(*source));
    if (!source)
        return NULL;

    source->list = rbh_iter_array(events, sizeof(*events), count);
    source->source = EVENT_LIST_SOURCE;
    if (!source->list) {
        free(source);
        return NULL;
    }

    return &source->source;
}

struct rbh_id *
fake_id(void)
{
    static struct lu_fid fid = {0};
    struct rbh_id *tmp;

    tmp = rbh_id_from_lu_fid(&fid);
    fid.f_oid++;

    return tmp;
}

void
fake_create(struct rbh_fsevent *fsevent, struct rbh_id *id,
            struct rbh_id *parent)
{
    memset(fsevent, 0, sizeof(*fsevent));

    fsevent->type = RBH_FET_LINK;
    fsevent->id = *id;
    fsevent->link.name = "test";
    fsevent->link.parent_id = parent;
}

void
fake_link(struct rbh_fsevent *fsevent, struct rbh_id *id, const char *name,
          struct rbh_id *parent)
{
    memset(fsevent, 0, sizeof(*fsevent));

    fsevent->type = RBH_FET_LINK;
    fsevent->id = *id;
    fsevent->link.name = name;
    fsevent->link.parent_id = parent;
}

void
fake_unlink(struct rbh_fsevent *fsevent, struct rbh_id *id, const char *name,
            struct rbh_id *parent)
{
    memset(fsevent, 0, sizeof(*fsevent));

    fsevent->type = RBH_FET_UNLINK;
    fsevent->id = *id;
    fsevent->link.name = name;
    fsevent->link.parent_id = parent;
}

void
fake_delete(struct rbh_fsevent *fsevent, struct rbh_id *id)
{
    memset(fsevent, 0, sizeof(*fsevent));

    fsevent->type = RBH_FET_DELETE;
    fsevent->id = *id;
}

struct rbh_value_map
make_upsert_statx(struct rbh_value_pair *pairs,
                  struct rbh_value *map_values,
                  uint32_t mask)
{
    struct rbh_value_map xattr = {
        .count = 1,
        .pairs = &pairs[0], /* "rbh-fsevents" */
    };
    struct rbh_value_pair rbh_fsevents_map = { /* pairs[0] */
        .key = "rbh-fsevents",
        .value = &map_values[0],
    };
    struct rbh_value rbh_fsevents_map_value = { /* map_values[0] */
        .type = RBH_VT_MAP,
        .map = {
            .count = 1,
            .pairs = &pairs[1], /* "statx" */
        },
    };
    struct rbh_value_pair statx_mask = {
        .key = "statx",
        .value = &map_values[1], /* mask */
    };
    struct rbh_value statx_mask_value = {
        .type = RBH_VT_UINT32,
        .uint32 = mask,
    };

    memcpy(&pairs[0], &rbh_fsevents_map, sizeof(rbh_fsevents_map));
    memcpy(&pairs[1], &statx_mask, sizeof(statx_mask));

    memcpy(&map_values[0], &rbh_fsevents_map_value,
           sizeof(rbh_fsevents_map_value));
    memcpy(&map_values[1], &statx_mask_value, sizeof(statx_mask_value));

    return xattr;
}

void
fake_upsert(struct rbh_fsevent *fsevent, struct rbh_id *id, uint32_t mask,
            struct rbh_statx *statx)
{
    struct rbh_value *map_values;
    struct rbh_value_pair *pairs;

    pairs = malloc(2 * sizeof(*pairs));
    ck_assert_ptr_nonnull(pairs);

    map_values = malloc(2 * sizeof(*map_values));
    ck_assert_ptr_nonnull(map_values);

    memset(fsevent, 0, sizeof(*fsevent));
    fsevent->id = *id;
    fsevent->upsert.statx = statx;
    fsevent->xattrs = make_upsert_statx(pairs, map_values, mask);
}
