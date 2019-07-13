/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 * 		      alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifndef ROBINHOOD_CHECK_COMPAT_H
#define ROBINHOOD_CHECK_COMPAT_H

#include <check.h>

#ifndef CHECK_CHECK_VERSION
#define CHECK_CHECK_VERSION(major, minor, micro) \
    (CHECK_MAJOR_VERSION > (major) \
     || (CHECK_MAJOR_VERSION == (major) && CHECK_MINOR_VERSION > (minor) \
         || (CHECK_MINOR_VERSION == (minor) && CHECK_MICRO_VERSION >= (micro))))
#endif

/* The following code is a partial backport of check */

/* Check: a unit test framework for C
 * Copyright (C) 2001, 2002 Arien Malec
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#if !CHECK_CHECK_VERSION(0, 9, 10) /* CHECK_VERSION < 0.9.10 */

/* Pointer comparison macros with improved output compared to ck_assert(). */
/* OP may only be == or !=  */
#define _ck_assert_ptr(X, OP, Y) do { \
      const void* _ck_x = (X); \
      const void* _ck_y = (Y); \
      ck_assert_msg(_ck_x OP _ck_y, \
                    "Assertion '%s' failed: %s == %#x, %s == %#x", \
                    #X" "#OP" "#Y, #X, _ck_x, #Y, _ck_y); \
} while (0)

/**
 * Check if two pointers are equal.
 *
 * If the two passed pointers are not equal, the test
 * fails.
 *
 * @param X pointer
 * @param Y pointer to compare against X
 *
 * @note If the check fails, the remaining of the test is aborted
 *
 * @since 0.9.10
 */
#define ck_assert_ptr_eq(X, Y) _ck_assert_ptr(X, ==, Y)

/**
 * Check if two pointers are not.
 *
 * If the two passed pointers are equal, the test fails.
 *
 * @param X pointer
 * @param Y pointer to compare against X
 *
 * @since 0.9.10
 */
#define ck_assert_ptr_ne(X, Y) _ck_assert_ptr(X, !=, Y)

#define _ck_assert_uint(X, OP, Y) do { \
  uintmax_t _ck_x = (X); \
  uintmax_t _ck_y = (Y); \
  ck_assert_msg(_ck_x OP _ck_y, \
                "Assertion '%s' failed: %s == %ju, %s == %ju", \
                #X" "#OP" "#Y, #X, _ck_x, #Y, _ck_y); \
} while (0)

/**
 * Check two unsigned integers to determine if X==Y
 *
 * If not X==Y, the test fails.
 *
 * @param X signed integer
 * @param Y signed integer to compare against X
 *
 * @note If the check fails, the remaining of the test is aborted
 *
 * @since 0.9.10
 */
#define ck_assert_uint_eq(X, Y) _ck_assert_uint(X, ==, Y)

#undef _ck_assert_str
#undef ck_assert_str_eq

/* String comparison macros with improved output compared to ck_assert() */
/* OP might be any operator that can be used in '0 OP strcmp(X,Y)' comparison. */
/* String pointer could be compared againts NULL with == (NULLEQ = 1) and != (NULLNE = 1) operators. */
/* The x and y parameter swap in strcmp() is needed to handle >, >=, <, <= operators. */
/* If the x or y parameter is NULL its value will be printed without quotes. */
#define _ck_assert_str(X, OP, Y, NULLEQ, NULLNE) do { \
  const char* _ck_x = (X); \
  const char* _ck_y = (Y); \
  const char* _ck_x_s; \
  const char* _ck_y_s; \
  const char* _ck_x_q; \
  const char* _ck_y_q; \
  if (_ck_x != NULL) { \
    _ck_x_q = "\""; \
    _ck_x_s = _ck_x; \
  } else { \
    _ck_x_q = ""; \
    _ck_x_s = "(null)"; \
  } \
  if (_ck_y != NULL) { \
    _ck_y_q = "\""; \
    _ck_y_s = _ck_y; \
  } else { \
    _ck_y_q = ""; \
    _ck_y_s = "(null)"; \
  } \
  ck_assert_msg( \
    (NULLEQ && (_ck_x == NULL) && (_ck_y == NULL)) || \
    (NULLNE && ((_ck_x == NULL) || (_ck_y == NULL)) && (_ck_x != _ck_y)) || \
    ((_ck_x != NULL) && (_ck_y != NULL) && (0 OP strcmp(_ck_y, _ck_x))), \
    "Assertion '%s' failed: %s == %s%s%s, %s == %s%s%s", \
    #X" "#OP" "#Y, \
    #X, _ck_x_q, _ck_x_s, _ck_x_q, \
    #Y, _ck_y_q, _ck_y_s, _ck_y_q); \
} while (0)

/**
 * Check two strings to determine if 0==strcmp(X,Y)
 *
 * If X or Y is NULL the test fails.
 * If not 0==strcmp(X,Y), the test fails.
 *
 * @param X string
 * @param Y string to compare against X
 *
 * @note If the check fails, the remaining of the test is aborted
 *
 * @since 0.9.6
 */
#define ck_assert_str_eq(X, Y) _ck_assert_str(X, ==, Y, 0, 0)

#endif /* CHECK_CHECK_VERSION(0, 9, 10) */

#if !CHECK_CHECK_VERSION(0, 11, 0) /* CHECK_VERSION < 0.11.0 */

/**
 * Check two strings to determine if 0==strcmp(X,Y) or if both are undefined
 *
 * If both X and Y are NULL the test passes. However, if only one is NULL
 * the test fails.
 * If not ((X==NULL)&&(Y==NULL)) || (0==strcmp(X,Y)), the test fails.
 *
 * @param X string
 * @param Y string to compare against X
 *
 * @note If the check fails, the remaining of the test is aborted
 *
 * @since 0.11.0
 */
#define ck_assert_pstr_eq(X, Y) _ck_assert_str(X, ==, Y, 1, 0)

/* Memory location comparison macros with improved output compared to ck_assert() */
/* OP might be any operator that can be used in '0 OP memcmp(X,Y,L)' comparison */
/* The x and y parameter swap in memcmp() is needed to handle >, >=, <, <= operators */
/* Output is limited to CK_MAX_ASSERT_MEM_PRINT_SIZE bytes */
#ifndef CK_MAX_ASSERT_MEM_PRINT_SIZE
#define CK_MAX_ASSERT_MEM_PRINT_SIZE 64
#endif

/* Memory location comparison macros with improved output compared to ck_assert() */
/* OP might be any operator that can be used in '0 OP memcmp(X,Y,L)' comparison */
/* The x and y parameter swap in memcmp() is needed to handle >, >=, <, <= operators */
/* Output is limited to CK_MAX_ASSERT_MEM_PRINT_SIZE bytes */
#ifndef CK_MAX_ASSERT_MEM_PRINT_SIZE
#define CK_MAX_ASSERT_MEM_PRINT_SIZE 64
#endif

#include <stdint.h>

#define _ck_assert_mem(X, OP, Y, L) do { \
    const uint8_t *_ck_x = (const uint8_t *)(X); \
    const uint8_t *_ck_y = (const uint8_t *)(Y); \
    size_t _ck_l = (L); \
    char _ck_x_str[CK_MAX_ASSERT_MEM_PRINT_SIZE * 2 + 1]; \
    char _ck_y_str[CK_MAX_ASSERT_MEM_PRINT_SIZE * 2 + 1]; \
    static const char _ck_hexdigits[] = "0123456789abcdef"; \
    size_t _ck_i; \
    size_t _ck_maxl = (_ck_l > CK_MAX_ASSERT_MEM_PRINT_SIZE) ? CK_MAX_ASSERT_MEM_PRINT_SIZE : _ck_l; \
    for (_ck_i = 0; _ck_i < _ck_maxl; _ck_i++) { \
        _ck_x_str[_ck_i * 2] = _ck_hexdigits[(_ck_x[_ck_i] >> 4) & 0xF]; \
        _ck_y_str[_ck_i * 2] = _ck_hexdigits[(_ck_y[_ck_i] >> 4) & 0xF]; \
        _ck_x_str[_ck_i * 2 + 1] = _ck_hexdigits[_ck_x[_ck_i] & 0xF]; \
        _ck_y_str[_ck_i * 2 + 1] = _ck_hexdigits[_ck_y[_ck_i] & 0xF]; \
    } \
    _ck_x_str[_ck_i * 2] = 0; \
    _ck_y_str[_ck_i * 2] = 0; \
    if (_ck_maxl != _ck_l) { \
        _ck_x_str[_ck_i * 2 - 2] = '.'; \
        _ck_y_str[_ck_i * 2 - 2] = '.'; \
        _ck_x_str[_ck_i * 2 - 1] = '.'; \
        _ck_y_str[_ck_i * 2 - 1] = '.'; \
    } \
    ck_assert_msg(0 OP memcmp(_ck_y, _ck_x, _ck_l), \
                  "Assertion '%s' failed: %s == \"%s\", %s == \"%s\"", \
                  #X" "#OP" "#Y, #X, _ck_x_str, #Y, _ck_y_str); \
} while (0)

/**
 * Check two memory locations to determine if 0==memcmp(X,Y,L)
 *
 * If not 0==memcmp(X,Y,L), the test fails.
 *
 * @param X memory location
 * @param Y memory location to compare against X
 *
 * @note If the check fails, the remaining of the test is aborted
 *
 * @since 0.11.0
 */
#define ck_assert_mem_eq(X, Y, L) _ck_assert_mem(X, ==, Y, L)

/* Pointer against NULL comparison macros with improved output
 *  * compared to ck_assert(). */
/* OP may only be == or !=  */
#define _ck_assert_ptr_null(X, OP) do { \
      const void* _ck_x = (X); \
      ck_assert_msg(_ck_x OP NULL, \
                    "Assertion '%s' failed: %s == %#x", \
                    #X" "#OP" NULL", #X, _ck_x); \
} while (0)

/**
 * Check if a pointer is equal to NULL.
 *
 * If X != NULL, the test fails.
 *
 * @param X pointer to compare against NULL
 *
 * @note If the check fails, the remaining of the test is aborted
 *
 * @since 0.11.0
 */
#define ck_assert_ptr_null(X) _ck_assert_ptr_null(X, ==)

/**
 * Check if a pointer is not equal to NULL.
 *
 * If X == NULL, the test fails.
 *
 * @param X pointer to compare against NULL
 *
 * @note If the check fails, the remaining of the test is aborted
 *
 * @since 0.11.0
 */
# define ck_assert_ptr_nonnull(X) _ck_assert_ptr_null(X, !=)

#endif /* CHECK_CHECK_VERSION(0, 11, 0) */

#endif
