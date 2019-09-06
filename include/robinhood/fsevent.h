/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifndef ROBINHOOD_FSEVENT_H
#define ROBINHOOD_FSEVENT_H

#include "robinhood/fsentry.h"

enum rbh_fsevent_type {
    RBH_FET_UPSERT,
    RBH_FET_LINK,
    RBH_FET_UNLINK,
    RBH_FET_DELETE,
};

struct statx;

struct rbh_fsevent {
    enum rbh_fsevent_type type;
    struct rbh_id id;
    union {
        /* RBH_FET_UPSERT */
        struct {
            const struct statx *statx; /* Nullable */
            const char *symlink; /* Nullable */
        } upsert;

        /* RBH_FET_LINK / RBH_FET_UNLINK */
        struct {
            const struct rbh_id *parent_id; /* NonNullable */
            const char *name; /* NonNullable */
        } link;
    };
};

struct rbh_fsevent *
rbh_fsevent_upsert_new(const struct rbh_id *id, const struct statx *statxbuf,
                       const char *symlink);

struct rbh_fsevent *
rbh_fsevent_link_new(const struct rbh_id *id, const struct rbh_id *parent_id,
                     const char *name);

struct rbh_fsevent *
rbh_fsevent_unlink_new(const struct rbh_id *id, const struct rbh_id *parent_id,
                       const char *name);

struct rbh_fsevent *
rbh_fsevent_delete_new(const struct rbh_id *id);

#endif
