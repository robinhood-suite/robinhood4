/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 * 		      alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifndef ROBINHOOD_FSENTRY_H
#define ROBINHOOD_FSENTRY_H

/** @file
 * fsentry is the generic name for a filesystem entry (file, dir, symlink, ...)
 */

/**
 * A unique identifier for an fsentry
 *
 * \c data is a series of \c size bytes (not null terminated).
 *
 * As a convention an ID with a \c size of 0 is used to represent a filesystem
 * root's parent fsentry (something that does not exist).
 */
struct rbh_id {
    char *data;
    size_t size;
};

struct statx;

/**
 * Any filesystem entry (file, dir, symlink, ...)
 *
 * \c mask is set to indicate which fields of an fsentry are set.
 *
 * To access the content of \c statx, either use the libc definition if it
 * provides one or include <robinhood/statx.h>.
 */
struct rbh_fsentry {
    unsigned int mask;
    struct rbh_id id;
    struct rbh_id parent_id;
    char *name;
    struct statx *statx;
};

enum rbh_fsentry_property {
    RBH_FP_ID           = 0x0001,
    RBH_FP_PARENT_ID    = 0x0002,
    RBH_FP_NAME         = 0x0004,
    RBH_FP_STATX        = 0x0008,

    RBH_FP_ALL          = 0x000f
};

/**
 * Create an fsentry in a single memory allocation
 *
 * @param id            id of the fsentry to create
 * @param parent_id     parent id of fsentry to create
 * @param name          name of the fsentry
 * @param stat          pointer to the struct stat of the fsentry to create
 *
 * @return              a pointer to a newly allocated struct rbh_fsentry on
 *                      success, NULL on error and errno is set appropriately
 *
 * @error ENOMEM        there was not enough memory available
 *
 * The returned fsentry can be freed with a single call to free().
 *
 * Any of \p id, \p parent_id, \p name and \p statx may be NULL, in which case
 * the corresponding fields in the returned fsentry are not set.
 */
struct rbh_fsentry *
rbh_fsentry_new(const struct rbh_id *id, const struct rbh_id *parent_id,
                const char *name, const struct statx *statx);

#endif
