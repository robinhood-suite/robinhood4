/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_VALUE_H
#define ROBINHOOD_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

struct rbh_value;

enum rbh_value_type {
    RBH_VT_BOOLEAN,
    RBH_VT_INT32,
    RBH_VT_UINT32,
    RBH_VT_INT64,
    RBH_VT_UINT64,
    RBH_VT_STRING,
    RBH_VT_BINARY,
    RBH_VT_REGEX,
    RBH_VT_SEQUENCE,
    RBH_VT_MAP,
};

struct rbh_value_pair {
    const char *key;
    const struct rbh_value *value;
};

struct rbh_value_map {
    const struct rbh_value_pair *pairs;
    size_t count;
};

enum rbh_regex_option {
    RBH_RO_CASE_INSENSITIVE = 0x1,

    RBH_RO_ALL = 0x1
};

struct rbh_value {
    enum rbh_value_type type;

    union {
        /* RBH_VT_BOOLEAN */
        bool boolean;

        /* RBH_VT_INT32 */
        int32_t int32;

        /* RBH_VT_UINT32 */
        uint32_t uint32;

        /* RBH_VT_INT64 */
        int64_t int64;

        /* RBH_VT_UINT64 */
        uint64_t uint64;

        /* RBH_VT_STRING */
        const char *string;

        /* RBH_VT_BINARY */
        struct {
            const char *data;
            size_t size;
        } binary;

        /* RBH_VT_REGEX */
        struct {
            const char *string;
            unsigned int options;
        } regex;

        /* RBH_VT_SEQUENCE */
        struct {
            const struct rbh_value *values;
            size_t count;
        } sequence;

        /* RBH_VT_MAP */
        struct rbh_value_map map;
    };
};

static const char * const VALUE_TYPE_NAMES[] = {
    [RBH_VT_BOOLEAN] = "boolean",
    [RBH_VT_INT32] = "int32",
    [RBH_VT_UINT32] = "unsigned int32",
    [RBH_VT_INT64] = "int64",
    [RBH_VT_UINT64] = "unsigned int64",
    [RBH_VT_STRING] = "string",
    [RBH_VT_BINARY] = "binary",
    [RBH_VT_REGEX] = "regex",
    [RBH_VT_SEQUENCE] = "sequence",
    [RBH_VT_MAP] = "map",
};

/**
 * Convert a rbh_value_type to a string
 *
 * @param type     the rbh_value_type to convert
 *
 * @return         "unknown" if the type is unknown, its string representation
 *                 otherwise
 */
static inline const char *
value_type2str(enum rbh_value_type type)
{
    if (type < RBH_VT_BOOLEAN || type > RBH_VT_MAP)
        return "unknown";

    return VALUE_TYPE_NAMES[type];
}

/**
 * Convert a string to a rbh_value_type
 *
 * @param string   the string to convert
 *
 * @return         -1 if the string does not translate to any type,
 *                 the translated type otherwise
 */
static inline enum rbh_value_type
str2value_type(const char *string)
{
    for (enum rbh_value_type i = RBH_VT_BOOLEAN; i <= RBH_VT_MAP; ++i)
        if (strcmp(string, VALUE_TYPE_NAMES[i]) == 0)
            return i;

    return -1;
}

/**
 * Create a new boolean value
 *
 * @param boolean   a boolean
 *
 * @return          a pointer to a newly allocated struct rbh_value on success,
 *                  -1 on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_value *
rbh_value_boolean_new(bool boolean);

/**
 * Create a new int32 value
 *
 * @param int32     an int32_t
 *
 * @return          a pointer to a newly allocated struct rbh_value on success,
 *                  -1 on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_value *
rbh_value_int32_new(int32_t int32);

/**
 * Create a new uint32 value
 *
 * @param uint32    an uint32_t
 *
 * @return          a pointer to a newly allocated struct rbh_value on success,
 *                  -1 on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_value *
rbh_value_uint32_new(uint32_t uint32);

/**
 * Create a new int64 value
 *
 * @param int64     an int64_t
 *
 * @return          a pointer to a newly allocated struct rbh_value on success,
 *                  -1 on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_value *
rbh_value_int64_new(int64_t int64);

/**
 * Create a new uint64 value
 *
 * @param uint64    an uint64_t
 *
 * @return          a pointer to a newly allocated struct rbh_value on success,
 *                  -1 on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_value *
rbh_value_uint64_new(uint64_t uint64);

/**
 * Create a new string value
 *
 * @param string    a (null-terminated) string
 *
 * @return          a pointer to a newly allocated struct rbh_value on success,
 *                  -1 on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_value *
rbh_value_string_new(const char *string);

/**
 * Create a new binary value
 *
 * @param data      a series of \p size bytes
 * @param size      the length of \p data in bytes
 *
 * @return          a pointer to a newly allocated struct rbh_value on success,
 *                  -1 on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_value *
rbh_value_binary_new(const char *data, size_t size);

/**
 * Create a new string value
 *
 * @param regex     a (null-terminated) regex
 * @param options   a bitmask of the regex options to set
 *
 * @return          a pointer to a newly allocated struct rbh_value on success,
 *                  -1 on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_value *
rbh_value_regex_new(const char *regex, unsigned int options);

/**
 * Create a new sequence value
 *
 * @param values    an array of struct rbh_value that will make up the sequence
 * @param count     the number of elements in \p values
 *
 * @return          a pointer to a newly allocated struct rbh_value on success,
 *                  -1 on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_value *
rbh_value_sequence_new(const struct rbh_value *values, size_t count);

/**
 * Create a new map value
 *
 * @param pairs     an array of struct rbh_value_pair that will make up the map
 * @param count     the number of elements in \p pairs
 *
 * @return          a pointer to a newly allocated struct rbh_value on success,
 *                  -1 on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_value *
rbh_value_map_new(const struct rbh_value_pair *pairs, size_t count);

/**
 * Check a struct rbh_value is valid
 *
 * @param value     the value to validate
 *
 * @return          0 if \p value is valid, -1 an error occurred (potentially
 *                  because \p value is invalid)
 *
 * @error EINVAL    \p value is invalid
 */
int
rbh_value_validate(const struct rbh_value *value);

/**
 * Make a standalone copy of a map
 *
 * @param dest      the map to copy to
 * @param src       the map to copy from
 * @param buffer    a buffer of at least \p bufsize bytes where any data \p src
 *                  references will be copied, on success it is updated to point
 *                  after the last byte that was copied in it
 * @param bufsize   a pointer to the number of bytes after \p buffer that are
 *                  valid, on success the value it points at is decremented by
 *                  the number of bytes that were written to \p data.
 *
 * @return          0 on success, -1 on error and errno is set appropriately
 *
 * @error EINVAL    \p src points at invalid data (eg. a struct rbh_value with
 *                  an incorrect type)
 * @error ENOBUFS   \p size is not big enough to store all the data \p src
 *                  points at
 *
 * After a successful return, \p dest and \p src do not share any of the data
 * they point at.
 */
int
value_map_copy(struct rbh_value_map *dest, const struct rbh_value_map *src,
               char **buffer, size_t *bufsize);

#endif
