/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_CHECK_MACROS_H
#define ROBINHOOD_CHECK_MACROS_H

#include "check-compat.h"
#include "robinhood/value.h"

#define _ck_assert_id(X, OP, Y) do { \
    _ck_assert_uint((X)->size, OP, (Y)->size); \
    if ((X)->size != 0) \
        _ck_assert_mem((X)->data, OP, (Y)->data, (X)->size); \
} while (0)

#define ck_assert_id_eq(X, Y) _ck_assert_id(X, ==, Y)

static inline const char *
value_type2str(enum rbh_value_type type)
{
    switch (type) {
    case RBH_VT_BOOLEAN:
        return "RBH_VT_BOOLEAN";
    case RBH_VT_INT32:
        return "RBH_VT_INT32";
    case RBH_VT_UINT32:
        return "RBH_VT_UINT32";
    case RBH_VT_INT64:
        return "RBH_VT_INT64";
    case RBH_VT_UINT64:
        return "RBH_VT_UINT64";
    case RBH_VT_STRING:
        return "RBH_VT_STRING";
    case RBH_VT_BINARY:
        return "RBH_VT_BINARY";
    case RBH_VT_REGEX:
        return "RBH_VT_REGEX";
    case RBH_VT_SEQUENCE:
        return "RBH_VT_SEQUENCE";
    case RBH_VT_MAP:
        return "RBH_VT_MAP";
    default:
        return "unknown";
    }
}

#define _ck_assert_value_type(X, OP, Y) \
    ck_assert_msg((X) OP (Y), ": %s is %s, %s is %s", \
                  #X, value_type2str(X), #Y, value_type2str(Y))

#define _ck_assert_value(X, OP, Y) do { \
    _ck_assert_value_type((X)->type, OP, (Y)->type); \
    switch ((X)->type) { \
    case RBH_VT_BOOLEAN: \
        _ck_assert_int((X)->boolean, OP, (Y)->boolean); \
        break; \
    case RBH_VT_INT32: \
        _ck_assert_int((X)->int32, OP, (Y)->int32); \
        break; \
    case RBH_VT_UINT32: \
        _ck_assert_uint((X)->uint32, OP, (Y)->uint32); \
        break; \
    case RBH_VT_INT64: \
        _ck_assert_int((X)->int64, OP, (Y)->int64); \
        break; \
    case RBH_VT_UINT64: \
        _ck_assert_uint((X)->uint64, OP, (Y)->uint64); \
        break; \
    case RBH_VT_STRING: \
        _ck_assert_str((X)->string, OP, (Y)->string, 0, 0); \
        break; \
    case RBH_VT_BINARY: \
        _ck_assert_uint((X)->binary.size, OP, (Y)->binary.size); \
        if ((X)->binary.size > 0) \
            _ck_assert_mem((X)->binary.data, OP, (Y)->binary.data, \
                           (X)->binary.size); \
        break; \
    case RBH_VT_REGEX: \
        _ck_assert_str((X)->regex.string, OP, (Y)->regex.string, 0, 0); \
        _ck_assert_int((X)->regex.options, OP, (Y)->regex.options); \
        break; \
    case RBH_VT_SEQUENCE: \
        _ck_assert_uint((X)->sequence.count, OP, (Y)->sequence.count); \
        /* Recursion has to be done manually */ \
        break; \
    case RBH_VT_MAP: \
        _ck_assert_uint((X)->map.count, OP, (Y)->map.count); \
        /* Recursion has to be done manually */ \
        break; \
    default: \
        ck_abort_msg("Unknown value type: %i", (X)->type); \
    } \
} while (0)

#define ck_assert_value_eq(X, Y) _ck_assert_value(X, ==, Y)

#define _ck_assert_value_pair(X, OP, Y) do { \
    _ck_assert_str((X)->key, OP, (Y)->key, 0, 0); \
    if ((X)->value == NULL) \
        _ck_assert_ptr((X)->value, OP, (Y)->value); \
    else \
        _ck_assert_value((X)->value, OP, (Y)->value); \
} while (0)

#define ck_assert_value_pair_eq(X, Y) _ck_assert_value_pair(X, ==, Y)

#define _ck_assert_value_map(X, OP, Y) do { \
    _ck_assert_uint((X)->count, OP, (Y)->count); \
    for (size_t i = 0; i < (X)->count; i++) \
        _ck_assert_value_pair(&(X)->pairs[i], OP, &(Y)->pairs[i]); \
} while (0)

#define ck_assert_value_map_eq(X, Y) _ck_assert_value_map(X, ==, Y)

#endif
