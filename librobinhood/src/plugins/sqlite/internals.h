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

#include <assert.h>
#include <jansson.h>
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

/**
 * Elements of this structure should only be accessed inside db_cursor.c
 */
struct sqlite_cursor {
    /** reference to the backend's open DB */
    sqlite3 *db;
    /** SQL statement to execute */
    sqlite3_stmt *stmt;
    /** index of current parameter to bind to query (cf. sqlite_bind_xxx)*/
    int index;
    /** column of the current element to read for the query's result's current
     * row
     */
    int col;
};

struct sqlite_backend {
    struct rbh_backend backend;
    struct sqlite_cursor cursor;
    const char *path;
    bool read_only;
    sqlite3 *db;
};

struct sqlite_iterator {
    struct rbh_mut_iterator iter;
    struct sqlite_cursor cursor;
    /** set to true the first time we reach the end of the rows. */
    bool done;
};

extern const struct rbh_backend_operations SQLITE_BACKEND_OPS;

ssize_t
sqlite_backend_update(void *backend, struct rbh_iterator *fsevents);

struct rbh_mut_iterator *
sqlite_backend_filter(void *backend, const struct rbh_filter *filter,
                      const struct rbh_filter_options *options,
                      const struct rbh_filter_output *output);

struct rbh_fsentry *
sqlite_backend_root(void *backend,
                   const struct rbh_filter_projection *projection);

struct rbh_backend *
sqlite_backend_branch(void *backend, const struct rbh_id *id, const char *path);

struct rbh_fsentry *
sqlite_branch_root(void *backend,
                   const struct rbh_filter_projection *projection);

void
sqlite_backend_destroy(void *backend);

int
sqlite_backend_insert_source(void *backend,
                             const struct rbh_value *backend_source);

struct rbh_value_map *
sqlite_backend_get_info(void *backend, int flags);

/* Open db connexion. Create the DB if it does not exist */
bool
sqlite_backend_open(struct sqlite_backend *sqlite,
                    const char *path,
                    bool read_only);

bool
sqlite_backend_dup(struct sqlite_backend *sqlite,
                   struct sqlite_backend *dup);

void
sqlite_backend_close(struct sqlite_backend *sqlite);

bool
sqlite_cursor_setup(struct sqlite_backend *backend,
                    struct sqlite_cursor *cursor);

bool
sqlite_cursor_fini(struct sqlite_cursor *cursor);

bool
sqlite_setup_query(struct sqlite_cursor *cursor, const char *query);

bool
sqlite_cursor_exec(struct sqlite_cursor *cursor);

bool
sqlite_cursor_step(struct sqlite_cursor *cursor);


bool
sqlite_cursor_bind_int64(struct sqlite_cursor *cursor, int64_t value);

int64_t
sqlite_cursor_get_int64(struct sqlite_cursor *cursor);

uint64_t
sqlite_cursor_get_uint64(struct sqlite_cursor *trans);

uint32_t
sqlite_cursor_get_uint32(struct sqlite_cursor *trans);

uint16_t
sqlite_cursor_get_uint16(struct sqlite_cursor *trans);

bool
sqlite_cursor_bind_string(struct sqlite_cursor *cursor, const char *string);

const char *
sqlite_cursor_get_string(struct sqlite_cursor *cursor);

bool
sqlite_cursor_bind_id(struct sqlite_cursor *cursor, const struct rbh_id *id);

bool
sqlite_cursor_bind_binary(struct sqlite_cursor *cursor,
                          const char *data,
                          size_t size);

bool
sqlite_cursor_get_id(struct sqlite_cursor *cursor, struct rbh_id *dst);

const char *
sqlite_xattr2json(const struct rbh_value_map *xattrs);

bool
sqlite_json2xattrs(const char *json, struct rbh_value_map *xattrs);

void
bin2hex(const struct rbh_value *binary, char *buf);

bool
setup_custom_functions(sqlite3 *db);

#endif
