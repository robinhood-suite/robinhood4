#include "fsevent_pool.h"
#include "hash.h"
#include "rbh_fsevent_utils.h"

#include <robinhood.h>

#include <assert.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

struct rbh_fsevent_pool {
    size_t size; /* maximum number of ids allowed in the pool */
    struct rbh_hashmap *pool; /* container of lists of events per id */
    struct rbh_sstack *list_container; /* container of list elements */
    struct rbh_list_node ids; /* list of rbh_id that were inserted in the pool
                               * ordered by time of insertion
                               */
    size_t count; /* total number of ids in the pool */
    struct rbh_list_node events; /* list of flushed events that the user of the
                                  * pool will iterate over
                                  */
    struct rbh_list_node free_fsevents; /* List of available
                                         * struct rbh_fsevent_node
                                         */
    struct rbh_list_node free_ids; /* List of available struct rbh_id_node */
    struct rbh_list_node free_nodes; /* List of available struct rbh_list_node
                                      */
};

struct rbh_list_node_wrapper {
    struct rbh_list_node link;
};

struct rbh_fsevent_node {
    struct rbh_fsevent fsevent;
    struct rbh_sstack *copy_data;
    struct rbh_list_node link;
};

struct rbh_id_node {
    const struct rbh_id *id;
    struct rbh_list_node link;
};

static bool
fsevent_pool_equals(const void *first, const void *second)
{
    return rbh_id_equal(first, second);
}

static size_t
fsevent_pool_hash_id(const void *key)
{
    return hash_id(key);
}

static size_t
fsevent_pool_hash_lu_id(const void *key)
{
    return hash_lu_id(key);
}

struct rbh_fsevent_pool *
rbh_fsevent_pool_new(size_t batch_size, struct source *source)
{
    struct rbh_fsevent_pool *pool;
    size_t (*hash_fn)(const void *);

    pool = malloc(sizeof(*pool));
    if (!pool)
        return NULL;

    // XXX the stack size should probably be a function of the batch_size
    pool->list_container = rbh_sstack_new(1 << 10);
    if (!pool->list_container) {
        free(pool);
        return NULL;
    }

    if (!strcmp(source->name, "lustre"))
        /* more efficient lustre specific hash function */
        hash_fn = fsevent_pool_hash_lu_id;
    else
        hash_fn = fsevent_pool_hash_id;

    pool->pool = rbh_hashmap_new(fsevent_pool_equals, hash_fn,
                                 batch_size * 100 / 70);
    if (!pool->pool) {
        int save_errno = errno;

        free(pool);
        rbh_sstack_destroy(pool->list_container);
        errno = save_errno;
        return NULL;
    }

    pool->size = batch_size;
    rbh_list_init(&pool->ids);
    pool->count = 0;
    rbh_list_init(&pool->events);
    rbh_list_init(&pool->free_ids);
    rbh_list_init(&pool->free_nodes);
    rbh_list_init(&pool->free_fsevents);

    return pool;
}

void
rbh_fsevent_pool_destroy(struct rbh_fsevent_pool *pool)
{
    struct rbh_fsevent_node *fsevent;
    struct rbh_id_node *id;

    // XXX this is useful to check for memory leaks but in practice we don't
    // need to free this as it may be slow to go through every elements of the
    // lists and the hashmap. We could have a compile time flag to remove this
    // part of the code and only run it in tests.
    rbh_list_foreach(&pool->events, fsevent, link)
        rbh_sstack_destroy(fsevent->copy_data);

    rbh_list_foreach(&pool->ids, id, link) {
        fsevent = (void *)rbh_hashmap_get(pool->pool, id->id);
        if (fsevent)
            rbh_sstack_destroy(fsevent->copy_data);
    }

    rbh_hashmap_destroy(pool->pool);
    rbh_sstack_destroy(pool->list_container);
    free(pool);
}

static bool
rbh_fsevent_pool_is_full(struct rbh_fsevent_pool *pool)
{
    return pool->count == pool->size;
}

static struct rbh_list_node *
event_list_alloc(struct rbh_fsevent_pool *pool)
{
    if (!rbh_list_empty(&pool->free_nodes)) {
        struct rbh_list_node *node;

        node = (void *)rbh_list_first(&pool->free_nodes,
                                      struct rbh_list_node_wrapper,
                                      link);
        rbh_list_del(node);
        return node;
    }

    return RBH_SSTACK_PUSH(pool->list_container, NULL,
                           sizeof(struct rbh_list_node));
}

static void
event_list_free(struct rbh_fsevent_pool *pool, struct rbh_list_node *node)
{
    rbh_list_add(&pool->free_nodes, node);
}

static struct rbh_fsevent_node *
fsevent_node_alloc(struct rbh_fsevent_pool *pool)
{
    struct rbh_fsevent_node *node;

    if (!rbh_list_empty(&pool->free_fsevents)) {

        node = (void *)rbh_list_first(&pool->free_fsevents,
                                      struct rbh_fsevent_node,
                                      link);
        rbh_list_del(&node->link);
        return node;
    }

    node = RBH_SSTACK_PUSH(pool->list_container, NULL,
                           sizeof(struct rbh_fsevent_node));

    node->copy_data = rbh_sstack_new(1 << 10);
    if (!node->copy_data)
        return NULL;

    return node;
}

static void
fsevent_node_free(struct rbh_fsevent_pool *pool, struct rbh_fsevent_node *node)
{
    rbh_list_del(&node->link);
    rbh_list_add(&pool->free_fsevents, &node->link);

    while (true) {
        size_t remaining;
        int rc;

        rbh_sstack_peek(node->copy_data, &remaining);
        if (remaining == 0)
            break;

        rc = rbh_sstack_pop(node->copy_data, remaining);
        assert(rc == 0);
    }
}

static struct rbh_id_node *
id_node_alloc(struct rbh_fsevent_pool *pool)
{
    if (!rbh_list_empty(&pool->free_ids)) {
        struct rbh_id_node *id_node;

        id_node = rbh_list_first(&pool->free_ids, struct rbh_id_node, link);
        if (id_node)
            rbh_list_del(&id_node->link);

        return id_node;
    }

    return RBH_SSTACK_PUSH(pool->list_container, NULL,
                           sizeof(struct rbh_id_node));
}

static void
id_node_free(struct rbh_fsevent_pool *pool, struct rbh_id_node *id)
{
    rbh_list_del(&id->link);
    rbh_list_add(&pool->free_ids, &id->link);
}

static int
rbh_fsevent_pool_insert_new_entry(struct rbh_fsevent_pool *pool,
                                  const struct rbh_fsevent *event)
{
    struct rbh_fsevent_node *node;
    struct rbh_list_node *events;
    struct rbh_id_node *id_node;
    int rc;

    events = event_list_alloc(pool);
    if (!events)
        return -1;

    node = fsevent_node_alloc(pool);
    if (!node)
        return -1;

    id_node = id_node_alloc(pool);
    if (!id_node)
        return -1;

    rbh_list_init(events);
    rbh_list_add(events, &node->link);
    /* the deep copy is necessary for 2 reasons:
     * 1. the source does not guarantee that the fsevents it generates will be
     * kept alive in the next call to rbh_iter_next on the source
     * 2. we will create new fsevents when merging duplicated ones
     */
    rbh_fsevent_deep_copy(&node->fsevent, event, node->copy_data);

    rc = rbh_hashmap_set(pool->pool, &node->fsevent.id, events);
    if (rc)
        return rc;

    id_node->id = &node->fsevent.id;

    rbh_list_add_tail(&pool->ids, &id_node->link);

    pool->count++;

    return 0;
}

static void
map_insert_pair(struct rbh_fsevent_node *cached_event,
                struct rbh_value_map *map,
                const struct rbh_value_pair *pair)
{
    struct rbh_value_pair *tmp;
    size_t *count_ref;
    void **ptr;

    tmp = RBH_SSTACK_PUSH(cached_event->copy_data, NULL,
                          sizeof(*tmp) * (map->count + 1));

    memcpy(tmp, map->pairs, map->count * sizeof(*map->pairs));
    memcpy(&tmp[map->count], pair, sizeof(*pair));

    // XXX this breaks the const constraint in struct rbh_value_map
    // The alternatives are:
    // 1. remove the const everywhere
    // 2. duplicate the fsevent completely each time we want to insert something
    // We decided that this was the "best" compromise.
    ptr = (void **)&map->pairs;
    count_ref = (size_t *)&map->count;

    *ptr = tmp;
    (*count_ref)++;
}

static void
sequence_insert_value(struct rbh_fsevent_node *cached_event,
                      struct rbh_value *sequence,
                      const struct rbh_value *value)
{
    struct rbh_value *tmp;
    size_t *count_ref;
    void **ptr;

    tmp = RBH_SSTACK_PUSH(
        cached_event->copy_data, NULL,
        (sequence->sequence.count + 1) * sizeof(*sequence->sequence.values)
        );

    memcpy(tmp, sequence->sequence.values,
           sequence->sequence.count * sizeof(*sequence->sequence.values));
    memcpy(&tmp[sequence->sequence.count], value, sizeof(*value));

    // XXX this breaks the const constraint in struct rbh_value_map
    // The alternatives are:
    // 1. remove the const everywhere
    // 2. duplicate the fsevent completely each time we want to insert something
    // We decided that this was the "best" compromise.
    ptr = (void **)&sequence->sequence.values;
    count_ref = (size_t *)&sequence->sequence.count;
    *ptr = tmp;
    (*count_ref)++;
}

/* return a pointer to the new "rbh_fsevents" map */
static const struct rbh_value_map *
insert_new_fsevents_map(struct rbh_fsevent_node *cached_event)
{
    /* "rbh-fsevents" may not exist if first xattr was complete */
    struct rbh_value rbh_fsevents_value = {
        .type = RBH_VT_MAP,
        .map = {
            .count = 0,
            .pairs = NULL,
        },
    };
    struct rbh_value_pair rbh_fsevents_pair = {
        .key = "rbh-fsevents",
        .value = RBH_SSTACK_PUSH(cached_event->copy_data, &rbh_fsevents_value,
                                 sizeof(rbh_fsevents_value)),
    };
    const struct rbh_value_pair *last_pair;
    size_t last_index;

    map_insert_pair(cached_event, &cached_event->fsevent.xattrs,
                    &rbh_fsevents_pair);

    last_index = cached_event->fsevent.xattrs.count - 1;
    last_pair = &cached_event->fsevent.xattrs.pairs[last_index];

    return &last_pair->value->map;
}

/* return a pointer to the new "xattrs" sequence */
static void
insert_new_xattrs_string_sequence(struct rbh_fsevent_node *cached_event,
                                  const struct rbh_value_map *rbh_fsevents,
                                  const struct rbh_value *first_string)
{
    struct rbh_value xattr_string = {
        .type = RBH_VT_STRING,
        .string = RBH_SSTACK_PUSH(cached_event->copy_data,
                                  first_string->string,
                                  strlen(first_string->string) + 1),
    };
    struct rbh_value xattrs_sequence = {
        .type = RBH_VT_SEQUENCE,
        .sequence = {
            .count = 1,
            .values = RBH_SSTACK_PUSH(cached_event->copy_data,
                                      &xattr_string, sizeof(xattr_string)),
        },
    };
    struct rbh_value_pair xattrs_pair = {
        .key = "xattrs",
        .value = RBH_SSTACK_PUSH(cached_event->copy_data,
                                 &xattrs_sequence, sizeof(xattrs_sequence)),
    };

    // XXX we discard const here
    map_insert_pair(cached_event, (struct rbh_value_map *)rbh_fsevents,
                    &xattrs_pair);
}

static void
insert_partial_xattr(struct rbh_fsevent_node *cached_event,
                     const struct rbh_value *partial_xattr)
{
    const struct rbh_value_map *rbh_fsevents_map;
    const struct rbh_value *xattrs_sequence;

    /* we have at least one xattr in the cached event */
    assert(cached_event->fsevent.xattrs.count >= 1);

    rbh_fsevents_map = rbh_fsevent_find_fsevents_map(&cached_event->fsevent);
    if (!rbh_fsevents_map) {
        /* "rbh-fsevents" may not exist if first xattr was complete */
        rbh_fsevents_map = insert_new_fsevents_map(cached_event);
        insert_new_xattrs_string_sequence(cached_event, rbh_fsevents_map,
                                          partial_xattr);
    }

    xattrs_sequence = rbh_map_find(rbh_fsevents_map, "xattrs");
    if (!xattrs_sequence) {
        /* the map may not exist if first event was "lustre" */
        insert_new_xattrs_string_sequence(cached_event, rbh_fsevents_map,
                                          partial_xattr);
        return;
    }

    struct rbh_value xattr_string = {
        .type = RBH_VT_STRING,
        .string = RBH_SSTACK_PUSH(cached_event->copy_data,
                                  partial_xattr->string,
                                  strlen(partial_xattr->string) + 1),
    };

    // XXX we discard const here
    sequence_insert_value(cached_event, (struct rbh_value *)xattrs_sequence,
                          &xattr_string);
}

static void
dedup_partial_xattr(struct rbh_fsevent_node *cached_event,
                    const struct rbh_value *partial_xattr)
{
    const struct rbh_value *cached_partial_xattr;

    assert(partial_xattr->type == RBH_VT_STRING);
    cached_partial_xattr = rbh_fsevent_find_partial_xattr(
        &cached_event->fsevent, partial_xattr->string
        );

    if (cached_partial_xattr)
        /* xattr found, do not add it to the cached fsevent */
        return;

    insert_partial_xattr(cached_event, partial_xattr);
}

static void
insert_enrich_element(struct rbh_fsevent_node *cached_event,
                      const struct rbh_value_pair *xattr)
{
    const struct rbh_value_map *rbh_fsevents_map;

    rbh_fsevents_map = rbh_fsevent_find_fsevents_map(&cached_event->fsevent);

    if (!rbh_fsevents_map) {
        /* "rbh-fsevents" may not exist if first xattr was complete */
        struct rbh_value rbh_fsevents_value = {
            .type = RBH_VT_MAP,
            .map = {
                .count = 1,
                .pairs = RBH_SSTACK_PUSH(cached_event->copy_data,
                                         xattr,
                                         sizeof(*xattr)),
            },
        };
        struct rbh_value_pair rbh_fsevents_map = {
            .key = "rbh-fsevents",
            .value = RBH_SSTACK_PUSH(cached_event->copy_data,
                                     &rbh_fsevents_value,
                                     sizeof(rbh_fsevents_value)),
        };

        map_insert_pair(cached_event, &cached_event->fsevent.xattrs,
                        &rbh_fsevents_map);

        return;
    }

    // XXX we discard const here
    map_insert_pair(cached_event, (struct rbh_value_map *)rbh_fsevents_map,
                    xattr);
}

static void
dedup_enrich_element(struct rbh_fsevent_node *cached_event,
                     const struct rbh_value_pair *xattr)
{
    const struct rbh_value_pair *cached_xattr;

    cached_xattr = rbh_fsevent_find_enrich_element(&cached_event->fsevent,
                                                   xattr->key);
    if (cached_xattr)
        /* xattr found, do not add it to the cached fsevent */
        return;

    insert_enrich_element(cached_event, xattr);
}

static void
insert_xattr(struct rbh_fsevent_node *cached_event,
             const struct rbh_value_pair *xattr)
{
    struct rbh_value_pair *tmp;
    struct rbh_value *value;

    tmp = RBH_SSTACK_PUSH(
        cached_event->copy_data, NULL,
        sizeof(*tmp) * (cached_event->fsevent.xattrs.count + 1)
    );

    memcpy(tmp, cached_event->fsevent.xattrs.pairs,
           cached_event->fsevent.xattrs.count * sizeof(*tmp));
    memcpy(&tmp[cached_event->fsevent.xattrs.count], xattr, sizeof(*xattr));

    value = RBH_SSTACK_PUSH(cached_event->copy_data, NULL, sizeof(*value));
    tmp[cached_event->fsevent.xattrs.count].value = value;

    rbh_value_deep_copy(value, xattr->value, cached_event->copy_data);

    cached_event->fsevent.xattrs.count++;
    cached_event->fsevent.xattrs.pairs = tmp;
}

static void
update_xattr_value(struct rbh_value *cached_xattr,
                   const struct rbh_value *new_xattr)
{
    /* overwrite the old value with the new one */
    memcpy(cached_xattr, new_xattr, sizeof(*new_xattr));
}

static void
dedup_xattr(struct rbh_fsevent_node *cached_event,
            const struct rbh_value_pair *xattr)
{
    const struct rbh_value_pair *cached_xattr;

    cached_xattr = rbh_fsevent_find_xattr(&cached_event->fsevent, xattr->key);
    if (cached_xattr) {
        if (strcmp(xattr->key, "nb_children") == 0)
            ((struct rbh_value *) xattr->value)->int64 +=
                ((struct rbh_value *) cached_xattr->value)->int64;

        /* xattr found, do not add it to the cached fsevent */
        update_xattr_value((void *)cached_xattr->value, xattr->value);
    } else {
        insert_xattr(cached_event, xattr);
    }
}

static void
dedup_xattrs(const struct rbh_fsevent *event,
             struct rbh_fsevent_node *cached_xattr)
{
    for (size_t i = 0; i < event->xattrs.count; i++) {
        const struct rbh_value_pair *xattr = &event->xattrs.pairs[i];

        if (!strcmp(xattr->key, "rbh-fsevents")) {
            const struct rbh_value_map *sub_map;

            assert(xattr->value->type == RBH_VT_MAP);
            sub_map = &xattr->value->map;

            for (size_t i = 0; i < sub_map->count; i++) {
                if (!strcmp(sub_map->pairs[i].key, "xattrs")) {
                    const struct rbh_value *to_enrich;

                    to_enrich = (void *)sub_map->pairs[i].value;

                    assert(to_enrich->type == RBH_VT_SEQUENCE);

                    for (size_t i = 0; i < to_enrich->sequence.count;
                         i++) {
                        const struct rbh_value *partial_xattr;

                        partial_xattr = &to_enrich->sequence.values[i];
                        dedup_partial_xattr(cached_xattr, partial_xattr);
                    }
                } else {
                    dedup_enrich_element(cached_xattr, &sub_map->pairs[i]);
                }
            }
        } else {
            dedup_xattr(cached_xattr, xattr);
        }
    }
}

static void
insert_symlink(struct rbh_fsevent_node *cached_event)
{
    const struct rbh_value_map *rbh_fsevents_map;
    struct rbh_value symlink_value = {
        .type = RBH_VT_STRING,
        .string = "symlink"
    };
    struct rbh_value_pair symlink_pair = {
        .key = "symlink",
        .value = RBH_SSTACK_PUSH(cached_event->copy_data, &symlink_value,
                                 sizeof(symlink_value))
    };

    rbh_fsevents_map = rbh_fsevent_find_fsevents_map(&cached_event->fsevent);
    assert(rbh_fsevents_map);

    // XXX we discard const here
    map_insert_pair(cached_event, (struct rbh_value_map *)rbh_fsevents_map,
                    &symlink_pair);
}

static void
dedup_upsert(const struct rbh_fsevent *event,
             struct rbh_fsevent_node *cached_upsert)
{
    const struct rbh_value_map *rbh_fsevents_map;

    if (event->upsert.statx) {
        if (cached_upsert->fsevent.upsert.statx)
            merge_statx((struct rbh_statx *)cached_upsert->fsevent.upsert.statx,
                        event->upsert.statx);
        else
            cached_upsert->fsevent.upsert.statx = event->upsert.statx;
    }

    rbh_fsevents_map = rbh_fsevent_find_fsevents_map(event);
    /* upsert events either contain "rbh-fsevents": "statx" or
     * "rbh-fsevents": "symlink".
     */
    if (!rbh_fsevents_map)
        return;

    for (size_t i = 0; i < rbh_fsevents_map->count; i++) {
        const struct rbh_value_pair *xattr = &rbh_fsevents_map->pairs[i];

        if (!strcmp(xattr->key, "statx")) {
            struct rbh_value *mask;
            mask = (struct rbh_value *)
            cached_upsert->fsevent.xattrs.pairs[0].value->map.pairs[0].value;

            mask->uint32 |=
                event->xattrs.pairs[0].value->map.pairs[i].value->uint32;

        } else if (!strcmp(xattr->key, "symlink")) {
            // XXX at most one symlink per inode
            insert_symlink(cached_upsert);
        }
    }
}

static void
remove_event_list(struct rbh_fsevent_pool *pool,
                  const struct rbh_id *id)
{
    struct rbh_id_node *elem, *tmp;

    rbh_hashmap_pop(pool->pool, id);
    pool->count--;

    rbh_list_foreach_safe(&pool->ids, elem, tmp, link) {
        if (rbh_id_equal(id, elem->id)) {
            // XXX we could keep a reference to this node in the
            // hash table's element
            rbh_list_del(&elem->link);
            break;
        }
    }
}

static bool
dedup_unlink_event(struct rbh_fsevent_pool *pool, struct rbh_list_node *events,
                   const struct rbh_fsevent *event)
{
    struct rbh_fsevent_node *link_fsevent = NULL;
    struct rbh_fsevent_node *node;

    rbh_list_foreach(events, node, link) {
        if (node->fsevent.type == RBH_FET_LINK &&
            !strcmp(event->link.name, node->fsevent.link.name) &&
            rbh_id_equal(event->link.parent_id,
                         node->fsevent.link.parent_id))
            link_fsevent = node;
    }

    if (!link_fsevent)
        return true;

    rbh_list_del(&link_fsevent->link);

    if (rbh_list_empty(events))
        remove_event_list(pool, &event->id);

    return false;
}

static bool
dedup_delete_event(struct rbh_fsevent_pool *pool, struct rbh_list_node *events,
                   const struct rbh_fsevent *event)
{
    struct rbh_fsevent_node *elem, *tmp;
    bool insert_delete = true;

    rbh_list_foreach_safe(events, elem, tmp, link) {
        if (elem->fsevent.type == RBH_FET_LINK)
            insert_delete = false;

        rbh_list_del(&elem->link);
    }

    if (!insert_delete) {
        remove_event_list(pool, &event->id);

        return false;
    }

    return true;
}

static bool
dedup_xattr_event(struct rbh_list_node *events,
                  const struct rbh_fsevent *fsevent)
{
    struct rbh_fsevent_node *xattr_event = NULL;
    struct rbh_fsevent_node *elem;

    // XXX instead of a list, we could have one xattr, a list of links, a
    // list of unlinks, one delete...
    rbh_list_foreach(events, elem, link) {
        if (elem->fsevent.type == RBH_FET_XATTR) {
            xattr_event = elem;
            /* with the dedup, only one xattr per event list */
            break;
        }
    }

    if (xattr_event) {
        dedup_xattrs(fsevent, xattr_event);

        return false;
    }

    return true;
}

static bool
dedup_upsert_event(struct rbh_list_node *events,
                   const struct rbh_fsevent *event)
{
    struct rbh_fsevent_node *upsert = NULL;
    struct rbh_fsevent_node *elem;

    rbh_list_foreach(events, elem, link) {
        if (elem->fsevent.type == RBH_FET_UPSERT) {
            upsert = elem;
            /* with the dedup, only one upsert per event list */
            break;
        }
    }

    if (upsert) {
        dedup_upsert(event, upsert);
        return false;
    }

    return true;
}

static void
move_list_node_at_tail(struct rbh_list_node *list, struct rbh_list_node *node)
{
    rbh_list_del(node);
    rbh_list_add_tail(list, node);
}

static struct rbh_id_node *
id_list_find(struct rbh_fsevent_pool *pool, const struct rbh_id *id)
{
    struct rbh_id_node *node;

    rbh_list_foreach(&pool->ids, node, link) {
        if (rbh_id_equal(id, node->id))
            return node;
    }

    return NULL;
}

static int
deduplicate_event(struct rbh_fsevent_pool *pool, struct rbh_list_node *events,
                  const struct rbh_fsevent *event)
{
    struct rbh_fsevent_node *node;
    bool should_insert = true;
    struct rbh_id_node *id;

    id = id_list_find(pool, &event->id);
    move_list_node_at_tail(&pool->ids, &id->link);

    switch (event->type) {
    case RBH_FET_UPSERT:
        should_insert = dedup_upsert_event(events, event);
        break;
    case RBH_FET_LINK:
        /* no dedup for link */
        break;
    case RBH_FET_UNLINK:
        should_insert = dedup_unlink_event(pool, events, event);
        break;
    case RBH_FET_DELETE:
        should_insert = dedup_delete_event(pool, events, event);
        break;
    case RBH_FET_XATTR:
        should_insert = dedup_xattr_event(events, event);
        break;
    }

    if (!should_insert)
        return 0;

    node = fsevent_node_alloc(pool);
    if (!node)
        return -1;

    rbh_fsevent_deep_copy(&node->fsevent, event, node->copy_data);
    if (event->type == RBH_FET_LINK)
        /* move links at the front to insert new entries before any other action
         */
        rbh_list_add(events, &node->link);
    else
        rbh_list_add_tail(events, &node->link);

    return 0;
}

enum fsevent_pool_push_res
rbh_fsevent_pool_push(struct rbh_fsevent_pool *pool,
                      const struct rbh_fsevent *event)
{
    struct rbh_list_node *events;
    int rc;

    if (rbh_fsevent_pool_is_full(pool)) {
        errno = ENOSPC;
        return POOL_ALREADY_FULL;
    }

    events = (void *)rbh_hashmap_get(pool->pool, &event->id);
    if (events) {
        rc = deduplicate_event(pool, events, event);
        if (rc)
            return POOL_INSERT_FAILED;

    } else {
        /* we do not insert NULL values in the map */
        assert(errno == ENOENT);
        rc = rbh_fsevent_pool_insert_new_entry(pool, event);
        if (rc)
            return POOL_INSERT_FAILED;

        errno = 0;
    }

    if (rbh_fsevent_pool_is_full(pool)) {
        /* Notify the caller that the pool is now full */
        errno = ENOSPC;
        return POOL_FULL;
    }

    return POOL_INSERT_OK;
}

struct rbh_iterator *
rbh_fsevent_pool_flush(struct rbh_fsevent_pool *pool)
{
    struct rbh_fsevent_node *elem, *tmp;

    rbh_list_foreach_safe(&pool->events, elem, tmp, link) {
        fsevent_node_free(pool, elem);
    }

    if (pool->count == 0)
        return NULL;

    while (pool->count > 0) {
        struct rbh_list_node *first_events;
        struct rbh_id_node *first_id;
        struct rbh_list_node *events;

        first_id = rbh_list_first(&pool->ids, struct rbh_id_node, link);
        first_events = (void *)rbh_hashmap_get(pool->pool, first_id->id);
        assert(first_events);

        rbh_list_splice_tail(&pool->events, first_events);

        id_node_free(pool, first_id);
        events = (void *)rbh_hashmap_pop(pool->pool, first_id->id);
        event_list_free(pool, events);
        pool->count--;
    }

    return rbh_iter_list(&pool->events,
                         offsetof(struct rbh_fsevent_node, link));
}
