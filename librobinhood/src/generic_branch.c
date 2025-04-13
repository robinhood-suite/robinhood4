/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>

#include "robinhood/itertools.h"
#include "robinhood/ringr.h"
#include "robinhood/backend.h"
#include "robinhood/statx.h"

/* This implementation is almost generic, except for the calls to
 * backend_filter() (calling rbh_backend_filter() instead is not an option as
 * it would lead to an infinite recursion).
 *
 * If another backend needs this code, it should include this source file after
 * approprately defining the backend_filter macro to the backend's
 * implementation of rbh_backend_filter. See the mongo and sqlite backend for
 * examples.
 */

enum ringr_reader_type {
    RRT_DIRECTORIES,
    RRT_FSENTRIES,
};

struct branch_iterator {
    struct rbh_mut_iterator iterator;

    struct rbh_backend *backend;
    struct rbh_filter *filter;
    struct rbh_filter_options options;
    struct rbh_filter_output output;

    struct rbh_mut_iterator *directories;
    struct rbh_mut_iterator *fsentries;
    struct rbh_fsentry *directory;

    struct rbh_ringr *ids[2];       /* indexed with enum ringr_reader_type */
    struct rbh_ringr *values[2];    /* indexed with enum ringr_reader_type */
    struct rbh_value value;
};

static enum ringr_reader_type
ringr_largest_reader(struct rbh_ringr *ringr[2])
{
    size_t size[2];

    rbh_ringr_peek(ringr[RRT_DIRECTORIES], &size[0]);
    rbh_ringr_peek(ringr[RRT_FSENTRIES], &size[1]);

    return size[0] > size[1] ? RRT_DIRECTORIES : RRT_FSENTRIES;
}

static struct rbh_mut_iterator *
_filter_child_fsentries(struct rbh_backend *backend, size_t id_count,
                        const struct rbh_value *id_values,
                        const struct rbh_filter *filter,
                        const struct rbh_filter_options *options,
                        const struct rbh_filter_output *output)
{
    const struct rbh_filter parent_id_filter = {
        .op = RBH_FOP_IN,
        .compare = {
            .field = {
                .fsentry = RBH_FP_PARENT_ID,
            },
            .value = {
                .type = RBH_VT_SEQUENCE,
                .sequence = {
                    .count = id_count,
                    .values = id_values,
                }
            },
        },
    };
    const struct rbh_filter *filters[2] = {
        &parent_id_filter,
        filter,
    };
    const struct rbh_filter and_filter = {
        .op = RBH_FOP_AND,
        .logical = {
            .count = 2,
            .filters = filters,
        },
    };

    return backend_filter(backend, &and_filter, options, output);
}

static struct rbh_mut_iterator *
filter_child_fsentries(struct rbh_backend *backend, struct rbh_ringr *_values,
                       struct rbh_ringr *_ids, const struct rbh_filter *filter,
                       const struct rbh_filter_options *options,
                       const struct rbh_filter_output *output)
{
    struct rbh_mut_iterator *iterator;
    struct rbh_value *values;
    size_t readable;
    size_t count;
    int rc;

    values = rbh_ringr_peek(_values, &readable);
    if (readable == 0) {
        errno = ENODATA;
        return NULL;
    }
    assert(readable % sizeof(*values) == 0);
    count = readable / sizeof(*values);

    iterator = _filter_child_fsentries(backend, count, values, filter, options,
                                       output);
    if (iterator == NULL)
        return NULL;

    /* IDs have variable size, we cannot just ack `count * <something>` bytes */
    readable = 0;
    for (size_t i = 0; i < count; ++i)
        readable += values[i].binary.size;

    /* Acknowledge data in both rings */
    rc = rbh_ringr_ack(_values, count * sizeof(*values));
    assert(rc == 0);
    rbh_ringr_ack(_ids, readable);
    assert(rc == 0);

    return iterator;
}

static const struct rbh_filter ISDIR_FILTER = {
    .op = RBH_FOP_EQUAL,
    .compare = {
        .field = {
            .fsentry = RBH_FP_STATX,
            .statx = RBH_STATX_TYPE,
        },
        .value = {
            .type = RBH_VT_INT32,
            .int32 = S_IFDIR,
        },
    },
};

static int
branch_iter_recurse(struct branch_iterator *iter)
{
    const struct rbh_filter_options OPTIONS = { 0 };
    const struct rbh_filter_output OUTPUT = {
        .type = RBH_FOT_PROJECTION,
        .projection = {
            .fsentry_mask = RBH_FP_ID,
        },
    };
    struct rbh_mut_iterator *_directories;
    struct rbh_mut_iterator *directories;

    _directories = filter_child_fsentries(iter->backend,
                                          iter->values[RRT_DIRECTORIES],
                                          iter->ids[RRT_DIRECTORIES],
                                          &ISDIR_FILTER, &OPTIONS, &OUTPUT);
    if (_directories == NULL)
        return -1;

    directories = rbh_mut_iter_chain(iter->directories, _directories);
    if (directories == NULL) {
        int save_errno = errno;

        rbh_mut_iter_destroy(_directories);
        errno = save_errno;
        return -1;
    }
    iter->directories = directories;

    return 0;
}

static struct rbh_mut_iterator *
_branch_next_fsentries(struct branch_iterator *iter)
{
    return filter_child_fsentries(iter->backend, iter->values[RRT_FSENTRIES],
                                  iter->ids[RRT_FSENTRIES], iter->filter,
                                  &iter->options, &iter->output);
}

static struct rbh_mut_iterator *
branch_next_fsentries(struct branch_iterator *iter)
{
    struct rbh_value *value = &iter->value;

    /* A previous call to branch_next_fsentries() may have been interrupted
     * before all the data that needed to be committed actually was.
     *
     * Resume the execution from where it stopped.
     */
    if (iter->directory != NULL) {
        if (iter->value.binary.data != NULL)
            goto record_rbh_value;
        goto record_id;
    }

    /* Build a list of directory ids */
    while (true) {
        const struct rbh_id *id;

        /* Fetch the next directory */
        iter->directory = rbh_mut_iter_next(iter->directories);
        if (iter->directory == NULL) {
            if (errno != ENODATA)
                return NULL;

            /* `iterator->directories' is exhausted, let's hydrate it */
            if (branch_iter_recurse(iter)) {
                if (errno != ENODATA)
                    return NULL;

                /* The traversal is complete */
                return _branch_next_fsentries(iter);
            }
            continue;
        }

record_id:
        id = &iter->directory->id;
        value->binary.data = rbh_ringr_push(*iter->ids, id->data, id->size);
        /* Record this directory for a later traversal */
        if (value->binary.data == NULL) {
            switch (errno) {
            case ENOBUFS: /* the ring is full */
                /* Should we traverse directories or fsentries? */
                switch (ringr_largest_reader(iter->ids)) {
                case RRT_DIRECTORIES:
                    if (branch_iter_recurse(iter)) {
                        /* the ring can't be full and empty at the same time */
                        assert(errno != ENODATA);
                        return NULL;
                    }
                    goto record_id;
                case RRT_FSENTRIES:
                    return _branch_next_fsentries(iter);
                }
                __builtin_unreachable();
            default:
                return NULL;
            }
        }
        value->binary.size = id->size;

        /* Recording the id first helps keep the code's complexity bearable
         *
         * If someone ever wants to do it the other way around, they should
         * keep in mind that:
         *   - there is no guarantee on how long an id is:
         *     1) although it is reasonable to assume an id is only a few bytes
         *        long (at the time of writing, ids are 16 bytes long), there is
         *        no limit to how big "a few bytes" might be
         *     2) different ids may have different sizes
         *   - list_child_fsentries() asssumes all the readable rbh_values
         *     in the value ring contain an id that points inside the id ring
         *   - rbh_ring_push() may fail (that one is obvious)
         *   - no matter the error, branch_next_fsentries() must be retryable
         */

record_rbh_value:
        /* Then, record the associated rbh_value in `iterator->values' */
        if (rbh_ringr_push(*iter->values, value, sizeof(*value)) == NULL) {
            switch (errno) {
            case ENOBUFS: /* the ring is full */
                /* Should we traverse directories or fsentries? */
                switch (ringr_largest_reader(iter->ids)) {
                case RRT_DIRECTORIES:
                    if (branch_iter_recurse(iter)) {
                        /* the ring can't be full and empty at the same time */
                        assert(errno != ENODATA);
                        return NULL;
                    }
                    goto record_rbh_value;
                case RRT_FSENTRIES:
                    return _branch_next_fsentries(iter);
                }
                __builtin_unreachable();
            default:
                return NULL;
            }
        }
        free(iter->directory);
    }
}

static void *
branch_iter_next(void *iterator)
{
    struct branch_iterator *iter = iterator;
    struct rbh_fsentry *fsentry;

    if (iter->fsentries == NULL) {
        iter->fsentries = branch_next_fsentries(iter);
        if (iter->fsentries == NULL)
            return NULL;
    }

    fsentry = rbh_mut_iter_next(iter->fsentries);
    if (fsentry != NULL)
        return fsentry;

    assert(errno);
    if (errno != ENODATA)
        return NULL;

    rbh_mut_iter_destroy(iter->fsentries);
    iter->fsentries = NULL;

    return branch_iter_next(iterator);
}

static void
branch_iter_destroy(void *iterator)
{
    struct branch_iterator *iter = iterator;

    rbh_ringr_destroy(iter->ids[0]);
    rbh_ringr_destroy(iter->ids[1]);
    rbh_ringr_destroy(iter->values[0]);
    rbh_ringr_destroy(iter->values[1]);

    if (iter->fsentries)
        rbh_mut_iter_destroy(iter->fsentries);
    if (iter->directories)
        rbh_mut_iter_destroy(iter->directories);
    free(iter->directory);
    free(iter->filter);
    free(iter);
}

static const struct rbh_mut_iterator_operations BRANCH_ITER_OPS = {
    .next    = branch_iter_next,
    .destroy = branch_iter_destroy,
};

static const struct rbh_mut_iterator BRANCH_ITERATOR = {
    .ops = &BRANCH_ITER_OPS,
};


/* Unlike rbh_backend_filter_one(), this function is about applying a filter to
 * a specific entry (identified by its ID) and see if it matches.
 */
static struct rbh_mut_iterator *
filter_one(void *backend, const struct rbh_id *id,
           const struct rbh_filter *filter,
           const struct rbh_filter_options *options,
           const struct rbh_filter_output *output)
{
    const struct rbh_filter id_filter = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_ID,
            },
            .value = {
                .type = RBH_VT_BINARY,
                .binary = {
                    .data = id->data,
                    .size = id->size,
                },
            },
        },
    };
    const struct rbh_filter *filters[2] = {
        &id_filter,
        filter,
    };
    const struct rbh_filter and_filter = {
        .op = RBH_FOP_AND,
        .logical = {
            .count = 2,
            .filters = filters,
        },
    };

    return backend_filter(backend, &and_filter, options, output);
}
#define VALUE_RING_SIZE (1 << 14) /* 16MB */
#define ID_RING_SIZE (1 << 14) /* 16MB */

struct rbh_mut_iterator *
generic_branch_backend_filter(void *backend, const struct rbh_filter *filter,
                              const struct rbh_filter_options *options,
                              const struct rbh_filter_output *output)
{
    const struct rbh_filter_projection ID_ONLY = {
        .fsentry_mask = RBH_FP_ID,
    };
    struct branch_iterator *iter;
    int save_errno = errno;

    /* The recursive traversal of the branch prevents a few features from
     * working out of the box.
     */
    if (options->skip || options->limit || options->sort.count) {
        errno = ENOTSUP;
        return NULL;
    }

    iter = malloc(sizeof(*iter));
    if (iter == NULL)
        return NULL;

    iter->directory = rbh_backend_root(backend, &ID_ONLY);
    if (iter->directory == NULL) {
        save_errno = errno;
        goto out_free_iter;
    }

    assert(iter->directory->mask & RBH_FP_ID);
    iter->fsentries = filter_one(backend, &iter->directory->id,
                                 filter, options, output);
    if (iter->fsentries == NULL) {
        save_errno = errno;
        goto out_free_directory;
    }

    errno = 0;
    iter->filter = filter ? rbh_filter_clone(filter) : NULL;
    if (iter->filter == NULL && errno != 0) {
        save_errno = errno;
        goto out_destroy_fsentries;
    }
    errno = save_errno;

    iter->values[0] = rbh_ringr_new(VALUE_RING_SIZE);
    if (iter->values[0] == NULL) {
        save_errno = errno;
        goto out_free_filter;
    }

    iter->values[1] = rbh_ringr_dup(iter->values[0]);
    if (iter->values[1] == NULL) {
        save_errno = errno;
        goto out_free_first_values_ringr;
    }

    iter->ids[0] = rbh_ringr_new(ID_RING_SIZE);
    if (iter->ids[0] == NULL) {
        save_errno = errno;
        goto out_free_second_values_ringr;
    }

    iter->ids[1] = rbh_ringr_dup(iter->ids[0]);
    if (iter->ids[1] == NULL) {
        save_errno = errno;
        goto out_free_first_ids_ringr;
    }

    iter->options = *options;
    iter->output = *output;
    iter->backend = backend;
    iter->iterator = BRANCH_ITERATOR;

    /* Setup `iter->value' for the first run of branch_next_fsentries() */
    iter->directories = rbh_mut_iter_array(NULL, 0, 0); /* Empty iterator */
    iter->value.type = RBH_VT_BINARY;
    iter->value.binary.data = NULL;

    return &iter->iterator;

out_free_first_ids_ringr:
    rbh_ringr_destroy(iter->ids[0]);
out_free_second_values_ringr:
    rbh_ringr_destroy(iter->values[1]);
out_free_first_values_ringr:
    rbh_ringr_destroy(iter->values[0]);
out_free_filter:
    free(iter->filter);
out_destroy_fsentries:
    rbh_mut_iter_destroy(iter->fsentries);
out_free_directory:
    free(iter->directory);
out_free_iter:
    free(iter);
    errno = save_errno;
    return NULL;
}
