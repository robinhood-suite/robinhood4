/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_BACKEND_H
#define ROBINHOOD_BACKEND_H

/** @file
 * A backend stores and serves a filesystem's metadata.
 *
 * \section errors Error handling
 *
 * For some backends, pinpointing exactly why an operation failed can be quite a
 * difficult task. In order to facilitate the development of new backends, the
 * following error handling mechanism is implemented:
 *
 * When a backend fails to handle an error properly, that is: set errno to a
 * documented value; it should set errno to \c RBH_BACKEND_ERROR and place an
 * explanation message in \c rbh_backend_error. This message should at best
 * explain what kind of error happened, at worst ask for forgiveness to users.
 *
 * \note Users! Application writers! It is your responsability to report to
 * backend maintainers when such an error occurs so that they may try their best
 * to come up with an updated error interface that allows you to gracefully
 * handle the error.
 */

#include <errno.h>

#include <sys/types.h>

#include "robinhood/filter.h"
#include "robinhood/fsentry.h"
#include "robinhood/fsevent.h"
#include "robinhood/iterator.h"

/**
 * Calls to a backend's methods may set errno to \c RBH_BACKEND_ERROR if they
 * are unable to clearly identify (and document) the cause of an error.
 * In this case, they must also write a short (< 512 bytes) message in the
 * thread-local variable \c rbh_backend_error.
 */
#define RBH_BACKEND_ERROR 1024

/**
 * This buffer is only filled with meaningful data after a failed call to a
 * backend's method set errno to \c RBH_BACKEND_ERROR. In this case, the buffer
 * contains a message that explains as best as possible why the operation
 * failed. This message should not be parsed in any way by applications and is
 * only meant to be printed to users.
 */
extern __thread char rbh_backend_error[512];

__attribute__((format(printf, 1, 2)))
void
rbh_backend_error_printf(const char *fmt, ...);

/**
 * A unique backend identifier
 *
 * Each backend can be identified either by its name or its backend ID.
 *
 * Out of the 256 available values [|0; 255|], the first 128 are reserved for
 * upstream backends.
 * Anyone willing to implement their own backend without contributing it back
 * upstream may choose an ID in the range [|128; 255|] with the knowledge that
 * no upstream backend shall conflict.
 *
 * Backend IDs are also used to identify and route backend options.
 * Options are stored in unsigned integers as follows:
 *
 * ```
 * 0               8               16
 * ---------------------------------
 * |   option_id   |  backend_id   |
 * ---------------------------------
 * ```
 *
 * ID 0 / \c RBH_BI_GENERIC is special. It is not linked to any particular
 * backend, and is used to generate generic option IDs, ie. options that can be
 * implemented irrespective of a backend's type.
 */
enum rbh_backend_id {
    RBH_BI_GENERIC, /* No backend should use this ID */

    /* Backends should declare their ID here */
    RBH_BI_POSIX,
    RBH_BI_POSIX_MPI,
    RBH_BI_MONGO,
    RBH_BI_LUSTRE,
    RBH_BI_LUSTRE_MPI,
    RBH_BI_HESTIA,
    RBH_BI_MPI_FILE,
    RBH_BI_RETENTION,
    RBH_BI_MFU,
    /* User defined backends should use an ID so that:
     * RBI_RESERVED_MAX < ID <= 255
     */
    RBH_BI_RESERVED_MAX = 127,
};

/**
 * A backend, ie. anything that can store/serve a filesystem's metadata
 */
struct rbh_backend {
    /** A unique identifier */
    unsigned int id;
    /** A unique name (mostly for logging purposes) */
    const char *name;
    /** A set of operations the backend implements */
    const struct rbh_backend_operations *ops;
};

/**
 * The fsentry fields a filter query should set
 *
 * Backends may choose to fill additionnal fields (eg. if the performance
 * penalty for this is deemed negligeable).
 * Fsentries may still be missing some (or all) of the required fields if
 * the data is missing from the backend.
 */
struct rbh_filter_projection {
    /** fsentry fields to fill */
    unsigned int fsentry_mask;
    /** statx fields to fill */
    unsigned int statx_mask;
    /** xattrs to fill (a map with a count of 0 means every xattr) */
    struct {
        /** inode xattrs to fill */
        struct rbh_value_map ns;
        /** namespace xattrs to fill */
        struct rbh_value_map inode;
    } xattrs;
};

/**
 * A filtering option the result of a filter query should be ordered by
 */
struct rbh_filter_sort {
    /** The field to use as a sorting key */
    struct rbh_filter_field field;
    /** Whether to order the results in ascending or descending order */
    bool ascending;
};

/**
 * Filtering options, to be used with rbh_backend_filter()
 */
struct rbh_filter_options {
    /** The number of fsentries to skip */
    size_t skip;
    /** The maximum number of fsentries to return (0 means unlimited) */
    size_t limit;
    /** Allow to skip error while generating fsevents */
    bool skip_error;
    /** Boolean to indicate it's a sync one */
    bool one;
    /** A sequence of sorting options */
    struct {
        const struct rbh_filter_sort *items;
        size_t count;
    } sort;
};

struct rbh_range_field {
    struct rbh_filter_field field;
    int64_t *boundaries;
    size_t boundaries_count;
};

enum field_accumulator {
    FA_NONE,
    FA_AVG,
    FA_COUNT,
    FA_MAX,
    FA_MIN,
    FA_SUM,
};

struct rbh_accumulator_field {
    enum field_accumulator accumulator;
    struct rbh_filter_field field;
};

/**
 * Grouping behaviour, to be used with rbh_backend_report()
 */
struct rbh_group_fields {
    struct rbh_range_field *id_fields;
    size_t id_count;

    struct rbh_accumulator_field *acc_fields;
    size_t acc_count;
};

/**
 * Determines if a rbh_filter_output aims to output a projection of fsentries or
 * simple values corresponding to aggregates of information in fsentries. The
 * later has no defined structure, as it's the caller's job to determine which
 * information to output.
 */
enum rbh_filter_output_type {
    RBH_FOT_PROJECTION, /* Projection output */
    RBH_FOT_VALUES,
};

/**
 * Output behaviour, to be used with rbh_backend_report() and
 * rbh_backend_filter()
 */
struct rbh_filter_output {
    enum rbh_filter_output_type type;
    union {
        /** Fsentry fields the query should set */
        struct rbh_filter_projection projection;

        struct {
            struct rbh_accumulator_field *fields;
            size_t count;
        } output_fields;
    };
};

/**
 * Determines which info will be displayed by rbh-info.
 */
enum rbh_info {
    RBH_CAPABILITIES_FLAG = 1 << 0,
    RBH_INFO_AVG_OBJ_SIZE = 1 << 1,
    RBH_INFO_COUNT        = 1 << 2,
    RBH_INFO_FIRST_SYNC   = 1 << 3,
    RBH_INFO_SIZE         = 1 << 4,
    RBH_INFO_LAST_SYNC    = 1 << 5,
};

/**
 * Operations backends implement
 *
 * Only the \c destroy() operation is mandatory, every other one may be set to
 * NULL to indicate it is not supported.
 *
 * Refer to the documentation of the matching rbh_backend_*() wrappers for more
 * information.
 */
struct rbh_backend_operations {
    int (*get_option)(
            void *backend,
            unsigned int option,
            void *data,
            size_t *data_size
            );
    int (*set_option)(
            void *backend,
            unsigned int option,
            const void *data,
            size_t data_size
            );
    ssize_t (*update)(
            void *backend,
            struct rbh_iterator *fsevents
            );
    int (*insert_metadata)(
            void *backend,
            struct rbh_value_map *map
            );
    struct rbh_backend *(*branch)(
            void *backend,
            const struct rbh_id *id,
            const char *path
            );
    struct rbh_fsentry *(*root)(
            void *backend,
            const struct rbh_filter_projection *projection
            );
    struct rbh_mut_iterator *(*filter)(
            void *backend,
            const struct rbh_filter *filter,
            const struct rbh_filter_options *options,
            const struct rbh_filter_output *output
            );
    struct rbh_mut_iterator *(*report)(
            void *backend,
            const struct rbh_filter *filter,
            const struct rbh_group_fields *group,
            const struct rbh_filter_options *options,
            const struct rbh_filter_output *output
            );
    int (*get_attribute)(
            void *backend,
            uint64_t flags,
            void *arg,
            struct rbh_value_pair *pairs,
            int available_pairs
            );
    struct rbh_value_map *(*get_info)(
            void *backend,
            int info_flags
            );
    void (*destroy)(
            void *backend
            );
};

/**
 * Compute the first option ID for a given backend ID
 *
 * @param backend_id    the ID of the backend whose first option ID to compute
 *
 * @return              the first option ID of the backend identified by
 *                      \p backend_id
 */
#define RBH_BO_FIRST(backend_id) (backend_id << 8)

/**
 * Extract the backend ID from an option ID
 *
 * @param option    the option ID from which to extract a backend ID
 *
 * @return          the backend ID \p option refers to
 */
#define RBH_BO_BACKEND_ID(option) (option >> 8)

/**
 * Generic options
 */
enum rbh_generic_backend_option {
    /** Deprecated options should be defined with this value.
     *
     * Until backends are confident that most, if not all, applications have
     * been recompiled against a version where the option is deprecated, they
     * should still handle the old value.
     */
    RBH_GBO_DEPRECATED = RBH_BO_FIRST(RBH_BI_GENERIC),
    /** Switch a backend to a "garbage collecting" mode
     *
     * When this option is set (on a backend that supports it), only entries
     * without any link in the namespace should be returned by subsequent call
     * to the `filter' operator.
     *
     * type: bool
     */
    RBH_GBO_GC,
};

/**
 * Generic backend "get_option" operation
 *
 * This function is meant only to be called from rbh_backend_get_option() and
 * provides a generic implementation for the options declared in
 * \c enum rbh_generic_backend_option.
 *
 * RBH_GBO_DEPRECATED   backends may set an option's ID to this to indicate that
 *                      the option is no longer supported.
 */
int
rbh_generic_backend_get_option(struct rbh_backend *backend, unsigned int option,
                               void *data, size_t *data_size);

/**
 * Get the value of a backend's option
 *
 * @param backend       the backend to query
 * @param option        the option whose value to get
 * @param data          a buffer where to put the value of \p option
 * @param data_size     the size of \p data, is updated to the number of bytes
 *                      set in \p data on success, or the minimum size \p data
 *                      must be for the operation to succeeds when an error
 *                      occurs and errno is set to EOVERFLOW.
 *
 * @return              0 on succes, -1 on error and errno is set appropriately
 *
 * @error EINVAL        \p option is an option for another backend
 * @error ENOTSUP       \p backend does not support the get_option operation or
 *                      \p option was deprecated and is not supported anymore
 * @error EOVERFLOW     \p data_size was too small for \p option (\p data_size
 *                      should contain the minimum number of bytes to allocate
 *                      \p data to so that the operation may succeed)
 * @error ENOPROTOOPT   invalid \p option (this is different than EINVAL in that
 *                      \p option looks like an option of \p backend but the
 *                      backend does not recognize it. One reason for that may
 *                      be that you are using an option not yet implemented by
 *                      the version \p backend you run against).
 */
static inline int
rbh_backend_get_option(struct rbh_backend *backend, unsigned int option,
                       void *data, size_t *data_size)
{
    if (RBH_BO_BACKEND_ID(option) == RBH_BI_GENERIC)
        return rbh_generic_backend_get_option(backend, option, data, data_size);

    if (RBH_BO_BACKEND_ID(option) != backend->id) {
        errno = EINVAL;
        return -1;
    }

    if (backend->ops->get_option == NULL) {
        errno = ENOTSUP;
        return -1;
    }

    return backend->ops->get_option(backend, option, data, data_size);
}

/**
 * Generic backend "set_option" operation
 *
 * This function is meant only to be called from rbh_backend_set_option() and
 * provides a generic implementation for the options declared in
 * \c enum rbh_generic_backend_option.
 *
 * RBH_GBO_DEPRECATED   backends may set an option's ID to this to indicate that
 *                      the option is no longer supported.
 */
int
rbh_generic_backend_set_option(struct rbh_backend *backend, unsigned int option,
                               const void *data, size_t data_size);

/**
 * Set the value of a backend's option
 *
 * @param backend       the backend to query
 * @param option        the option whose value to set
 * @param data          a buffer containing the new value of \p option
 * @param data_size     the size of \p data
 *
 * @return              0 on succes, -1 on error and errno is set appropriately
 *
 * @error EINVAL        \p option is an option for another backend, or \p data
 *                      or \p data_size is invalid.
 * @error ENOTSUP       \p backend does not support the set_option operation,
 *                      or \p option or \p data is not supported.
 * @error ENOPROTOOPT   invalid \p option (this is different than EINVAL in that
 *                      \p option looks like an option of \p backend but the
 *                      backend does not recognize it. One reason for that may
 *                      be that you are using an option not yet implemented by
 *                      the version \p backend you run against).
 */
static inline int
rbh_backend_set_option(struct rbh_backend *backend, unsigned int option,
                       const void *data, size_t data_size)
{
    if (RBH_BO_BACKEND_ID(option) == RBH_BI_GENERIC)
        return rbh_generic_backend_set_option(backend, option, data, data_size);

    if (RBH_BO_BACKEND_ID(option) != backend->id) {
        errno = EINVAL;
        return -1;
    }

    if (backend->ops->set_option == NULL) {
        errno = ENOTSUP;
        return -1;
    }

    return backend->ops->set_option(backend, option, data, data_size);
}

/**
 * Apply a series of fsevents on a backend
 *
 * @param backend   the backend on which to apply \p fsevents
 * @param fsevents  an iterator over fsevents to apply on \p backend
 *
 * @return          the number of applied fsevents on success, -1 on error and
 *                  errno is set appropriately
 *
 * @error ENOTSUP   \p backend does not support updating
 *
 * This function only returns successfully if it (sucessfully) processed each
 * fsevent of \p fsevents.
 *
 * It is the caller's responsibility to destroy \p fsevents.
 *
 * This function may fail and set errno to any error number specifically
 * documented by \p backend.
 */
static inline ssize_t
rbh_backend_update(struct rbh_backend *backend, struct rbh_iterator *fsevents)
{
    if (backend->ops->update == NULL) {
        errno = ENOTSUP;
        return -1;
    }

    return backend->ops->update(backend, fsevents);
}

/**
 * Insert metadata into a writing backend
 *
 * @param backend     the backend in which to insert metadata
 * @param log_map     rbh_value_map with metadata to insert
 *
 * @return            0 if inserted successfully, 1 on error
 */
static inline int
rbh_backend_insert_metadata(struct rbh_backend *backend,
                            struct rbh_value_map *map)
{
    if (backend->ops->insert_metadata == NULL) {
        errno = ENOTSUP;
        return -1;
    }

    return backend->ops->insert_metadata(backend, map);
}

/**
 * Create a sub-backend instance
 *
 * For example, if a backend A contains:
 *
 * ```
 *             a
 *       -------------
 *       b           c
 *    -------     -------
 *    d     e     f     g
 *  ----- ----- ----- -----
 *  h   i j   k l   m n   o
 * ```
 *
 * Then rbh_backend_branch(A, b) contains:
 *
 * ```
 *      b
 *   -------
 *   d     e
 * ----- -----
 * h   i j   k
 * ```
 *
 * @param backend   the backend to extract a new backend from
 * @param id        the id of the fsentry to use as the root of the new backend
 * @param path      the path of the fsentry to use as the root of the new
 *                  backend, may be NULL
 *
 * @return          a pointer to a newly allocated struct rbh_backend on
 *                  success, NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 * @error ENOTSUP   \p backend does not support subsetting
 *
 * If \p id is not the id of a directory the result is undefined.
 *
 * If the entry \p id refers to is removed from \p backend, future operations
 * may fail with errno set to ENOENT.
 *
 * If \p id is not the id of an entry which \p backend currently manages or used
 * to manage, the result is undefined.
 */
static inline struct rbh_backend *
rbh_backend_branch(struct rbh_backend *backend, const struct rbh_id *id,
                   const char *path)
{
    if (backend->ops->branch == NULL) {
        errno = ENOTSUP;
        return NULL;
    }
    return backend->ops->branch(backend, id, path);
}

/**
 * Return the root of a backend
 *
 * @param backend       the backend whose root to return
 * @param projection    the fields of the root to fill
 *
 * @return              the root of \p backend
 *
 * @error ENOTSUP       this operation is not implemented by \p backend
 * @error ENOMEM        there was not enough memory available
 */
static inline struct rbh_fsentry *
rbh_backend_root(struct rbh_backend *backend,
                 const struct rbh_filter_projection *projection)
{
    if (backend->ops->root == NULL) {
        errno = ENOTSUP;
        return NULL;
    }
    return backend->ops->root(backend, projection);
}

/**
 * Return an iterator over fsentries that match a set of criteria
 *
 * @param backend   the backend from which to fetch fsentries
 * @param filter    a set of criteria that the returned fsentries must match
 * @param options   a set of filtering options (must not be NULL)
 * @param output    the information to be outputted
 *
 * @return          an iterator over mutable fsentries on success, NULL on error
 *                  and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 * @error ENOTSUP   \p backend does not support filtering fsentries
 *
 * This function may fail and set errno to any error number specifically
 * documented by \p backend.
 */
static inline struct rbh_mut_iterator *
rbh_backend_filter(struct rbh_backend *backend, const struct rbh_filter *filter,
                   const struct rbh_filter_options *options,
                   const struct rbh_filter_output *output)
{
    if (backend->ops->filter == NULL) {
        errno = ENOTSUP;
        return NULL;
    }
    return backend->ops->filter(backend, filter, options, output);
}

/**
 * Return an iterator over fsentries that aggregate information
 *
 * @param backend   the backend from which to fetch fsentries
 * @param filter    a set of criteria that the returned fsentries must match
 * @param options   a set of filtering options (must not be NULL)
 *
 * @return          an iterator over mutable fsentries on success, NULL on error
 *                  and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 * @error ENOTSUP   \p backend does not support filtering fsentries
 *
 * This function may fail and set errno to any error number specifically
 * documented by \p backend.
 */
static inline struct rbh_mut_iterator *
rbh_backend_report(struct rbh_backend *backend, const struct rbh_filter *filter,
                   const struct rbh_group_fields *group,
                   const struct rbh_filter_options *options,
                   const struct rbh_filter_output *output)
{
    if (backend->ops->report == NULL) {
        errno = ENOTSUP;
        return NULL;
    }
    return backend->ops->report(backend, filter, group, options, output);
}


#define RBH_ATTR_MASK     0x00ffffffffffffff
#define RBH_ATTR_SHIFT    56

#define RBH_ATTR_FLAG(flag) ((uint64_t)(flag & RBH_ATTR_MASK))
#define RBH_ATTR_BACKEND(flags) (flags >> RBH_ATTR_SHIFT)
#define RBH_ATTR_BACKEND_VALUE(backend) \
    ((uint64_t)(RBH_BI_ ## backend) << RBH_ATTR_SHIFT)

/**
 * Retrieve specific attributes from a backend
 *
 * @param backend   the backend from which to fetch attributes
 * @param attr_name the name of the fetched attribute
 * @param arg       \p backend-specific arguments
 * @param pairs     filled pairs with names and values of fetched attributes
 * @param available_pairs
 *                  number of pairs that can be filled with additionnal keys
 *                  and values
 *
 * @return          -1 if an error occured,
 *                  the number of fetched attributes otherwise
 */
static inline int
rbh_backend_get_attribute(struct rbh_backend *backend, uint64_t flags,
                          void *arg, struct rbh_value_pair *pairs,
                          int available_pairs)
{
    if (backend->ops->get_attribute == NULL) {
        errno = ENOTSUP;
        return -1;
    }
    return backend->ops->get_attribute(backend, flags, arg, pairs,
                                       available_pairs);
}

/**
 * Retrieve info from a given backend (such as size, first sync, last sync...)
 *
 * @param backend   a pointer to the struct rbh_backend to reclaim
 * @param info      enum of the required infos
 */

static inline struct rbh_value_map *
rbh_backend_get_info(struct rbh_backend *backend, int info_flags)
{
    if (backend->ops->get_info == NULL) {
        errno = ENOTSUP;
        return NULL;
    }
    return backend->ops->get_info(backend, info_flags);
}

/**
 * Free resources associated to a struct rbh_backend
 *
 * @param backend   a pointer to the struct rbh_backend to reclaim
 */
static inline void
rbh_backend_destroy(struct rbh_backend *backend)
{
    return backend->ops->destroy(backend);
}

/**
 * Retrieve a single fsentry
 *
 * @param backend       the backend from which to retrieve the fsentry
 * @param filter        the filter to use
 * @param projection    fields of the fsentry to fill
 *
 * @return              a pointer to a newly allocated fsentry that matches
 *                      \p filter on success, NULL on error and errno is set
 *                      appropriately
 *
 * @error ENOENT        no fsentry in \p backend matches \p filter
 *
 * This function is a thin wrapper around rbh_backend_filter(). As such, any
 * comment that applies to rbh_backend_filter() also applies to this function.
 *
 * In particular, this function may fail and set errno for any of the errors
 * specified for rbh_backend_filter().
 */
struct rbh_fsentry *
rbh_backend_filter_one(struct rbh_backend *backend,
                       const struct rbh_filter *filter,
                       const struct rbh_filter_projection *projection);

/**
 * Retrieve an fsentry from a backend using its path
 *
 * @param backend       the backend from which to retrieve the fsentry
 * @param path          the path of the fsentry to return
 * @param projection    fields of the fsentry to fill
 *
 * @return              a pointer to a newly allocated fsentry whose path (in
 *                      \p backend) matches \p path, on success, NULL on error,
 *                      and errno is set appropriately.
 *
 * @error ENODATA       \p backend is missing information to convert \p path
 *                      into an fsentry
 * @error ENOENT        no fsentry in \p backend has a path that matches \p path
 *
 * This function is a wrapper around rbh_backend_filter(). As such, any comment
 * that applies to rbh_backend_filter() also applies to this function.
 *
 * In particular, this function may fail and set errno for any of the errors
 * specified for rbh_backend_filter().
 */
struct rbh_fsentry *
rbh_backend_fsentry_from_path(struct rbh_backend *backend, const char *path,
                              const struct rbh_filter_projection *projection);

#endif
