/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_SQLITE_INTERNALS_H
#define ROBINHOOD_SQLITE_INTERNALS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sqlite3.h>

#include <robinhood/backends/sqlite.h>
#include <robinhood/utils.h>

#define sqlite_fail(fmt, ...)                                    \
    ({                                                           \
        rbh_backend_error_printf("sqlite: " fmt __VA_OPT__(,) __VA_ARGS__); \
        false;                                                   \
    })

#define sqlite_db_fail(db, fmt, ...)                  \
        sqlite_fail(fmt ": %s" __VA_OPT__(,)          \
                    __VA_ARGS__, sqlite3_errmsg(db));

struct sqlite_backend {
    struct rbh_backend backend;
    const char *path;
    sqlite3 *db;
};

ssize_t
sqlite_backend_update(void *backend, struct rbh_iterator *fsevents);

void
sqlite_backend_destroy(void *backend);

/* Open db connexion. Create the DB if it does not exist */
bool
sqlite_backend_open(struct sqlite_backend *sqlite,
                    const char *path,
                    bool read_only);

void
sqlite_backend_close(struct sqlite_backend *sqlite);

#endif
