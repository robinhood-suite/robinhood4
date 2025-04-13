/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

typedef bool (*bit_operator_t)(int64_t, int64_t);

static void
sqlite_generic_bit_operator(sqlite3_context *ctxt, sqlite3_value **values,
                            bit_operator_t operator)
{
    int64_t value;
    int64_t mask;

    switch (sqlite3_value_type(values[0])) {
        case SQLITE_INTEGER:
            value = sqlite3_value_int64(values[0]);
            mask = sqlite3_value_int64(values[1]);

            return sqlite3_result_int(ctxt, operator(value, mask));
        case SQLITE_NULL:
            /* No field in the xattr, return false */
            return sqlite3_result_int(ctxt, 0);
        case SQLITE_BLOB:
        case SQLITE_TEXT:
            /* XXX will this be useful at some point? */
            abort();
        case SQLITE_FLOAT:
        default:
            abort();
    }
}

static bool
bit_any_set(int64_t value, int64_t mask)
{
    return value & mask;
}

static bool
bit_any_clear(int64_t value, int64_t mask)
{
    return (~value & mask);
}

static bool
bit_all_set(int64_t value, int64_t mask)
{
    return !((value & mask) ^ mask);
}

static bool
bit_all_clear(int64_t value, int64_t mask)
{
    return !(value & mask);
}

#define make_custom_bit_operator(op) \
    static void \
    sqlite_##op(sqlite3_context *ctxt, \
                int argc, \
                sqlite3_value **values) \
    { \
        sqlite_generic_bit_operator(ctxt, values, (op)); \
    }

make_custom_bit_operator(bit_any_set);
make_custom_bit_operator(bit_any_clear);
make_custom_bit_operator(bit_all_set);
make_custom_bit_operator(bit_all_clear);

typedef void (*custum_func_t)(sqlite3_context *, int, sqlite3_value **);

struct sql_func {
    const char *name;
    custum_func_t function;
};

static bool
setup_custom_function(sqlite3 *db, struct sql_func *func)
{
    int rc;

    rc = sqlite3_create_function(db, func->name, 2, SQLITE_ANY,
                                 NULL, func->function, NULL, NULL);
    if (rc != SQLITE_OK)
        return sqlite_db_fail(db, "failed to setup custom db functions");

    return true;
}

bool
setup_custom_functions(sqlite3 *db)
{
    struct sql_func functions[] = {
        { "bit_any_set",   sqlite_bit_any_set   },
        { "bit_all_set",   sqlite_bit_all_set   },
        { "bit_all_clear", sqlite_bit_all_clear },
        { "bit_any_clear", sqlite_bit_any_clear },
    };

    for (size_t i = 0; i < sizeof(functions) / sizeof(functions[0]); i++)
        if (!setup_custom_function(db, &functions[i]))
            return false;

    return true;
}
