/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifndef ROBINHOOD_BACKEND_H
#define ROBINHOOD_BACKEND_H

#include <errno.h>

#include <sys/types.h>

#include "robinhood/filter.h"
#include "robinhood/fsentry.h"
#include "robinhood/fsevent.h"
#include "robinhood/iterator.h"

/**
 * For some backends, pinpointing exactly why an operation failed may be a quite
 * difficult task. In order to facilitate the development of new backends, the
 * following error handling mechanism is implemented:
 *
 * When a backend fails to handle an error properly, that is: set errno to a
 * documented value; it should set errno to \c RBH_BACKEND_ERROR and place an
 * explanation message in \c rbh_backend_error. This message should at best
 * explain what kind of error happened, at worst ask for forgiveness to users.
 *
 * Users! Application writers! It is your responsability to report to backend
 * maintainers when such an error occurs so that they may try their best to
 * come up with an updated error interface that allows you to gracefully handle
 * the error.
 */

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

struct rbh_backend {
    unsigned int id;
    const char *name;
    const struct rbh_backend_operations *ops;
};

enum rbh_backend_id {
    RBH_BI_GENERIC, /* No backend should use this ID */

    /* Backends should declare their ID here */
    RBH_BI_POSIX,

    /* User defined backends should use an ID so that:
     * RBI_RESERVED_MAX < ID <= 255
     */
    RBH_BI_RESERVED_MAX = 127,
};

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
    struct rbh_backend *(*branch)(
            void *backend,
            const struct rbh_id *id
            );
    struct rbh_mut_iterator *(*filter_fsentries)(
            void *backend,
            const struct rbh_filter *filter,
            unsigned int fsentry_mask,
            unsigned int statx_mask
            );
    void (*destroy)(
            void *backend
            );
};

/* Options are stored in unsigned integers as follows:
 *
 * 0               8               16
 * ---------------------------------
 * |   option_id   |  backend_id   |
 * ---------------------------------
 *
 */

#define RBH_BO_FIRST(backend_id) (backend_id << 8)
#define RBH_BO_BACKEND_ID(option) (option >> 8)

enum rbh_generic_backend_option {
    RBH_GBO_DEPRECATED = RBH_BO_FIRST(RBH_BI_GENERIC),
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
 * @error ENOTPROTOOPT  invalid \p option (this is different than EINVAL in that
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
 * @error ENOTPROTOOPT  invalid \p option (this is different than EINVAL in that
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
 * It is the caller's responsibility to close \p fsevents.
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
 * Create a sub-backend instance
 *
 * @param backend   the backend to extract a new backend from
 * @param id        the id of the fsentry to use as the root of the new backend
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
rbh_backend_branch(struct rbh_backend *backend, const struct rbh_id *id)
{
    if (backend->ops->branch == NULL) {
        errno = ENOTSUP;
        return NULL;
    }
    return backend->ops->branch(backend, id);
}

/**
 * Returns fsentries that match a set of criteria
 *
 * @param backend       the backend from which to fetch fsentries
 * @param filter        a set of criteria that the returned fsentries must match
 * @param fsentry_mask  a bitmask of the fields to set for each fsentry
 * @param statx_mask    a bitmask of the fields to set in the statx field of
 *                      each fsentry (ignored if RBH_FP_STATX is not set in
 *                      \p fsentry_mask).
 *
 * @return              an iterator over mutable fsentries on success, NULL on
 *                      error and errno is set appropriately
 *
 * @error ENOTSUP       \p backend does not support filtering fsentries
 *
 * Backends may choose to return more (reps. less) fields in fsentries than is
 * required by \p fsentry_mask and \p statx_mask because the extra information
 * comes at little to no cost (resp. the backend is missing this information).
 *
 * It is the caller's responsibility to check the corresponding masks in the
 * returned fsentries to know whether or not a given field is set and safe to
 * access.
 *
 * This function may fail and set errno to any error number specifically
 * documented by \p backend.
 */
static inline struct rbh_mut_iterator *
rbh_backend_filter_fsentries(struct rbh_backend *backend,
                             const struct rbh_filter *filter,
                             unsigned int fsentry_mask,
                             unsigned int statx_mask)
{
    if (backend->ops->filter_fsentries == NULL) {
        errno = ENOTSUP;
        return NULL;
    }
    return backend->ops->filter_fsentries(backend, filter, fsentry_mask,
                                          statx_mask);
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
 * Retrieve an fsentry from a backend using its path
 *
 * @param backend       the backend from which to retrieve the fsentry
 * @param path          the path of the fsentry to return (will be modified and
 *                      should not be used anymore)
 * @param fsentry_mask  a bitmask of the fields to set in the returned fsentry
 * @param statx_mask    a bitmask of the fields to set in the statx field of
 *                      the returned fsentry (ignored if RBH_FP_STATX is not set
 *                      in \p fsentry_mask).
 *
 * @return              a pointer to a newly allocated fsentry whose path (in
 *                      \p backend) matches \p path, on success, NULL on error,
 *                      and errno is set appropriately.
 *
 * @error ENODATA       \p backend is missing information to convert \p path
 *                      into an fsentry
 * @error ENOENT        no fsentry in \p backend has a path that matches \p path
 *
 * Just like with rbh_backend_filter_fsentries, the returned fsentry may have
 * more or less fields.
 *
 * It is the caller's responsibility to check the corresponding masks in the
 * returned fsentries to know whether or not a given field is set and safe to
 * access.
 *
 * This function may also fail and set errno for any of the errors specified for
 * the routine rbh_backend_filter_fsentries().
 */
struct rbh_fsentry *
rbh_backend_fsentry_from_path(struct rbh_backend *backend, char *path,
                              unsigned int fsentry_mask,
                              unsigned int statx_mask);

#endif
