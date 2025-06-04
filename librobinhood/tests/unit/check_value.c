/* This file is part of the RobinHood Library
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "robinhood/value.h"

#include "check-compat.h"
#include "check_macros.h"
#include "utils.h"
#include "value.h"

/*----------------------------------------------------------------------------*
 |                          rbh_value_boolean_new()                           |
 *----------------------------------------------------------------------------*/

START_TEST(rvbn_false)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_BOOLEAN,
        .boolean = false,
    };
    struct rbh_value *value;

    value = rbh_value_boolean_new(VALUE.boolean);
    ck_assert_ptr_nonnull(value);

    ck_assert_value_eq(&VALUE, value);

    free(value);
}
END_TEST

START_TEST(rvbn_true)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_BOOLEAN,
        .boolean = true,
    };
    struct rbh_value *value;

    value = rbh_value_boolean_new(VALUE.boolean);
    ck_assert_ptr_nonnull(value);

    ck_assert_value_eq(&VALUE, value);

    free(value);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                           rbh_value_int32_new()                            |
 *----------------------------------------------------------------------------*/

START_TEST(rvi32n_min)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_INT32,
        .int32 = INT32_MIN,
    };
    struct rbh_value *value;

    ck_assert_uint_eq(sizeof(VALUE.int32), sizeof(int32_t));
    ck_assert_int_lt(VALUE.int32, 0);

    value = rbh_value_int32_new(VALUE.int32);
    ck_assert_ptr_nonnull(value);

    ck_assert_value_eq(value, &VALUE);

    free(value);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                           rbh_value_uint32_new()                           |
 *----------------------------------------------------------------------------*/

START_TEST(rvu32n_max)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_UINT32,
        .uint32 = UINT32_MAX,
    };
    struct rbh_value *value;

    ck_assert_uint_eq(sizeof(VALUE.uint32), sizeof(uint32_t));

    value = rbh_value_uint32_new(VALUE.uint32);
    ck_assert_ptr_nonnull(value);

    ck_assert_value_eq(value, &VALUE);

    free(value);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                           rbh_value_int64_new()                            |
 *----------------------------------------------------------------------------*/

START_TEST(rvi64n_min)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_INT64,
        .int64 = INT64_MIN,
    };
    struct rbh_value *value;

    ck_assert_uint_eq(sizeof(VALUE.int64), sizeof(int64_t));
    ck_assert_int_lt(VALUE.int64, 0);

    value = rbh_value_int64_new(VALUE.int64);
    ck_assert_ptr_nonnull(value);

    ck_assert_value_eq(value, &VALUE);

    free(value);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                           rbh_value_uint64_new()                           |
 *----------------------------------------------------------------------------*/

START_TEST(rvu64n_max)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_UINT64,
        .uint64 = UINT64_MAX,
    };
    struct rbh_value *value;

    ck_assert_uint_eq(sizeof(VALUE.uint64), sizeof(uint64_t));

    value = rbh_value_uint64_new(VALUE.uint64);
    ck_assert_ptr_nonnull(value);

    ck_assert_value_eq(value, &VALUE);

    free(value);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                           rbh_value_string_new()                           |
 *----------------------------------------------------------------------------*/

START_TEST(rvstrn_basic)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_STRING,
        .string = "abcdefg",
    };
    struct rbh_value *value;

    value = rbh_value_string_new(VALUE.string);
    ck_assert_ptr_nonnull(value);

    ck_assert_value_eq(value, &VALUE);
    ck_assert_ptr_ne(value->string, VALUE.string);

    free(value);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                           rbh_value_binary_new()                           |
 *----------------------------------------------------------------------------*/

START_TEST(rvbn_basic)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = "abcdefg",
            .size = 8,
        },
    };
    struct rbh_value *value;

    value = rbh_value_binary_new(VALUE.binary.data, VALUE.binary.size);
    ck_assert_ptr_nonnull(value);

    ck_assert_value_eq(value, &VALUE);
    ck_assert_ptr_ne(value->binary.data, VALUE.binary.data);

    free(value);
}
END_TEST

START_TEST(rvbn_empty)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_BINARY,
        .binary = {
            .size = 0,
        },
    };
    struct rbh_value *value;

    value = rbh_value_binary_new(NULL, 0);
    ck_assert_ptr_nonnull(value);

    ck_assert_value_eq(value, &VALUE);

    free(value);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                           rbh_value_regex_new()                            |
 *----------------------------------------------------------------------------*/

START_TEST(rvrn_basic)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_REGEX,
        .regex = {
            .string = "abcdefg",
            .options = RBH_RO_ALL,
        },
    };
    struct rbh_value *value;

    value = rbh_value_regex_new(VALUE.regex.string, VALUE.regex.options);
    ck_assert_ptr_nonnull(value);

    ck_assert_value_eq(value, &VALUE);
    ck_assert_ptr_ne(value->regex.string, VALUE.regex.string);

    free(value);
}
END_TEST

START_TEST(rvrn_bad_option)
{
    errno = 0;
    ck_assert_ptr_null(rbh_value_regex_new("abcdefg", RBH_RO_ALL + 1));
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                          rbh_value_sequence_new()                          |
 *----------------------------------------------------------------------------*/

START_TEST(rvseqn_basic)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = "abcdefg",
            .size = 8,
        },
    };
    const struct rbh_value SEQUENCE = {
        .type = RBH_VT_SEQUENCE,
        .sequence = {
            .values = &VALUE,
            .count = 1,
        },
    };
    struct rbh_value *value;

    value = rbh_value_sequence_new(SEQUENCE.sequence.values,
                                   SEQUENCE.sequence.count);
    ck_assert_ptr_nonnull(value);

    ck_assert_value_eq(value, &SEQUENCE);
    ck_assert_value_eq(value->sequence.values, &VALUE);
    ck_assert_ptr_ne(value->sequence.values, &VALUE);

    free(value);
}
END_TEST

START_TEST(rvseqn_empty)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_SEQUENCE,
        .sequence = {
            .count = 0,
        },
    };
    struct rbh_value *value;

    value = rbh_value_sequence_new(NULL, 0);
    ck_assert_ptr_nonnull(value);

    ck_assert_value_eq(value, &VALUE);

    free(value);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                            rbh_value_map_new()                             |
 *----------------------------------------------------------------------------*/

START_TEST(rvmn_basic)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = "abcdefg",
            .size = 8,
        },
    };
    const struct rbh_value_pair PAIR = {
        .key = "abcdefg",
        .value = &VALUE,
    };
    const struct rbh_value MAP = {
        .type = RBH_VT_MAP,
        .map = {
            .pairs = &PAIR,
            .count = 1,
        },
    };
    struct rbh_value *value;

    value = rbh_value_map_new(MAP.map.pairs, MAP.map.count);
    ck_assert_ptr_nonnull(value);

    ck_assert_value_eq(value, &MAP);
    ck_assert_value_map_eq(&value->map, &MAP.map);
    ck_assert_ptr_ne(value->map.pairs, &PAIR);

    free(value);
}
END_TEST

START_TEST(rvmn_empty)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_MAP,
        .map = {
            .count = 0,
        },
    };
    struct rbh_value *value;

    value = rbh_value_map_new(NULL, 0);
    ck_assert_ptr_nonnull(value);

    ck_assert_value_eq(value, &VALUE);

    free(value);
}
END_TEST

START_TEST(rvmn_misaligned)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_INT32,
        .int32 = 0,
    };
    const struct rbh_value_pair PAIR = {
        .key = "abcdef", /* strlen(key) + 1 == 7 (not aligned) */
        .value = &VALUE,
    };
    const struct rbh_value MAP = {
        .type = RBH_VT_MAP,
        .map = {
            .pairs = &PAIR,
            .count = 1,
        },
    };
    struct rbh_value *value;

    value = rbh_value_map_new(MAP.map.pairs, MAP.map.count);
    ck_assert_ptr_nonnull(value);

    ck_assert_value_eq(value, &MAP);
    ck_assert_value_map_eq(&value->map, &MAP.map);

    free(value);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                           value_map_data_size()                            |
 *----------------------------------------------------------------------------*/

/* value_map_data_size() is not part of RobinHood's public API
 *
 * It is already tested by the tests above, we just need to care about corner
 * cases here (ie. invalid value types) */

START_TEST(vmds_bad_type)
{
    const struct rbh_value VALUE = {
        .type = INT_MAX,
    };
    const struct rbh_value_pair PAIR = {
        .key = "test",
        .value = &VALUE,
    };
    const struct rbh_value_map MAP = {
        .pairs = &PAIR,
        .count = 1,
    };

    errno = 0;
    ck_assert_int_eq(value_map_data_size(&MAP), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(vmds_bad_type_in_sequence)
{
    const struct rbh_value VALUE = {
        .type = INT_MAX,
    };
    const struct rbh_value SEQUENCE = {
        .type = RBH_VT_SEQUENCE,
        .sequence = {
            .values = &VALUE,
            .count = 1,
        },
    };
    const struct rbh_value_pair PAIR = {
        .key = "test",
        .value = &SEQUENCE,
    };
    const struct rbh_value_map MAP = {
        .pairs = &PAIR,
        .count = 1,
    };

    errno = 0;
    ck_assert_int_eq(value_map_data_size(&MAP), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                              value_map_copy()                              |
 *----------------------------------------------------------------------------*/

/* value_map_copy() is not part of RobinHood's public API */

START_TEST(vmc_too_small_for_array_of_pairs)
{
    const struct rbh_value_map MAP = {
        .pairs = NULL,
        .count = 1,
    };
    char buffer[sizeof(*MAP.pairs) - 1];
    size_t size = sizeof(buffer);
    char *data = buffer;

    errno = 0;
    ck_assert_int_eq(value_map_copy(NULL, &MAP, &data, &size), -1);
    ck_assert_int_eq(errno, ENOBUFS);
}
END_TEST

START_TEST(vmc_too_small_for_key)
{
    const char KEY[] = "abcdefg";
    const struct rbh_value_pair PAIR = {
        .key = KEY,
        .value = NULL,
    };
    const struct rbh_value_map MAP = {
        .pairs = &PAIR,
        .count = 1,
    };
    char buffer[sizeof(PAIR) + sizeof(KEY) - 1];
    size_t size = sizeof(buffer);
    char *data = buffer;

    errno = 0;
    ck_assert_int_eq(value_map_copy(NULL, &MAP, &data, &size), -1);
    ck_assert_int_eq(errno, ENOBUFS);
}
END_TEST

START_TEST(vmc_too_small_for_bare_value)
{
    const char KEY[] = "abcdefg";
    const struct rbh_value VALUE = {};
    const struct rbh_value_pair PAIR = {
        .key = KEY,
        .value = &VALUE,
    };
    const struct rbh_value_map MAP = {
        .pairs = &PAIR,
        .count = 1,
    };
    char buffer[sizeof(PAIR) + sizeof(KEY) + sizeof(VALUE) - 1];
    size_t size = sizeof(buffer);
    char *data = buffer;

    errno = 0;
    ck_assert_int_eq(value_map_copy(NULL, &MAP, &data, &size), -1);
    ck_assert_int_eq(errno, ENOBUFS);
}
END_TEST

START_TEST(vmc_bad_type)
{
    const struct rbh_value VALUE = {
        .type = INT_MAX,
    };
    const char KEY[] = "abcdefg";
    const struct rbh_value_pair PAIR = {
        .key = KEY,
        .value = &VALUE,
    };
    const struct rbh_value_map MAP = {
        .pairs = &PAIR,
        .count = 1,
    };
    char buffer[sizeof(PAIR) + sizeof(KEY) + sizeof(VALUE)];
    size_t size = sizeof(buffer);
    char *data = buffer;

    errno = 0;
    ck_assert_int_eq(value_map_copy(NULL, &MAP, &data, &size), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(vmc_too_small_for_binary_value)
{
    const char DATA[] = "abcdefg";
    const struct rbh_value VALUE = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = DATA,
            .size = sizeof(DATA),
        },
    };
    const char KEY[] = "hijklmn";
    const struct rbh_value_pair PAIR = {
        .key = KEY,
        .value = &VALUE,
    };
    const struct rbh_value_map MAP = {
        .pairs = &PAIR,
        .count = 1,
    };
    char buffer[sizeof(PAIR) + sizeof(KEY) + sizeof(VALUE) + sizeof(DATA) - 1];
    size_t size = sizeof(buffer);
    char *data = buffer;

    errno = 0;
    ck_assert_int_eq(value_map_copy(NULL, &MAP, &data, &size), -1);
    ck_assert_int_eq(errno, ENOBUFS);
}
END_TEST

START_TEST(vmc_too_small_for_string_value)
{
    const char STRING[] = "abcdefg";
    const struct rbh_value VALUE = {
        .type = RBH_VT_STRING,
        .string = STRING,
    };
    const char KEY[] = "hijklmn";
    const struct rbh_value_pair PAIR = {
        .key = KEY,
        .value = &VALUE,
    };
    const struct rbh_value_map MAP = {
        .pairs = &PAIR,
        .count = 1,
    };
    char buffer[sizeof(PAIR) + sizeof(KEY) + sizeof(VALUE) + sizeof(STRING)
                - 1];
    size_t size = sizeof(buffer);
    char *data = buffer;

    errno = 0;
    ck_assert_int_eq(value_map_copy(NULL, &MAP, &data, &size), -1);
    ck_assert_int_eq(errno, ENOBUFS);
}
END_TEST

START_TEST(vmc_too_small_for_regex_value)
{
    const char REGEX[] = "abcdefg";
    const struct rbh_value VALUE = {
        .type = RBH_VT_REGEX,
        .regex = {
           .string = REGEX,
        },
    };
    const char KEY[] = "hijklmn";
    const struct rbh_value_pair PAIR = {
        .key = KEY,
        .value = &VALUE,
    };
    const struct rbh_value_map MAP = {
        .pairs = &PAIR,
        .count = 1,
    };
    char buffer[sizeof(PAIR) + sizeof(KEY) + sizeof(VALUE) + sizeof(REGEX) - 1];
    size_t size = sizeof(buffer);
    char *data = buffer;

    errno = 0;
    ck_assert_int_eq(value_map_copy(NULL, &MAP, &data, &size), -1);
    ck_assert_int_eq(errno, ENOBUFS);
}
END_TEST

START_TEST(vmc_too_small_for_bare_sequence_values)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_SEQUENCE,
        .sequence = {
            .count = 1,
        },
    };
    const char KEY[] = "abcdefg";
    const struct rbh_value_pair PAIR = {
        .key = KEY,
        .value = &VALUE,
    };
    const struct rbh_value_map MAP = {
        .pairs = &PAIR,
        .count = 1,
    };
    char buffer[sizeof(PAIR) + sizeof(KEY) + sizeof(VALUE) + sizeof(VALUE) -1];
    size_t size = sizeof(buffer);
    char *data = buffer;

    errno = 0;
    ck_assert_int_eq(value_map_copy(NULL, &MAP, &data, &size), -1);
    ck_assert_int_eq(errno, ENOBUFS);
}
END_TEST

START_TEST(vmc_too_small_for_sequence_values)
{
    const char DATA[] = "abcdefg";
    const struct rbh_value VALUE = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = DATA,
            .size = sizeof(DATA),
        },
    };
    const struct rbh_value SEQUENCE = {
        .type = RBH_VT_SEQUENCE,
        .sequence = {
            .values = &VALUE,
            .count = 1,
        },
    };
    const char KEY[] = "hijklmn";
    const struct rbh_value_pair PAIR = {
        .key = KEY,
        .value = &SEQUENCE,
    };
    const struct rbh_value_map MAP = {
        .pairs = &PAIR,
        .count = 1,
    };
    char buffer[sizeof(PAIR) + sizeof(KEY) + sizeof(SEQUENCE) + sizeof(VALUE)
                + sizeof(DATA) - 1];
    size_t size = sizeof(buffer);
    char *data = buffer;

    errno = 0;
    ck_assert_int_eq(value_map_copy(NULL, &MAP, &data, &size), -1);
    ck_assert_int_eq(errno, ENOBUFS);
}
END_TEST

START_TEST(vmc_too_small_for_map_value)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_MAP,
        .map = {
            .count = 1,
        },
    };
    const char KEY[] = "abcdefg";
    const struct rbh_value_pair PAIR = {
        .key = KEY,
        .value = &VALUE,
    };
    const struct rbh_value_map MAP = {
        .pairs = &PAIR,
        .count = 1,
    };
    char buffer[sizeof(PAIR) + sizeof(KEY) + sizeof(VALUE) + sizeof(PAIR) - 1];
    size_t size = sizeof(buffer);
    char *data = buffer;

    errno = 0;
    ck_assert_int_eq(value_map_copy(NULL, &MAP, &data, &size), -1);
    ck_assert_int_eq(errno, ENOBUFS);
}
END_TEST

START_TEST(vmc_misaligned_buffer)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_UINT32,
        .uint32 = 0,
    };
    const char KEY[] = "abcdefg";
    const struct rbh_value_pair PAIR = {
        .key = KEY,
        .value = &VALUE,
    };
    const struct rbh_value_map MAP = {
        .pairs = &PAIR,
        .count = 1,
    };
    struct rbh_value_map map;
    char buffer[sizeof(PAIR) + sizeof(KEY) + sizeof(VALUE) + sizeof(PAIR)];
    size_t size = sizeof(buffer) - 1;
    char *data = buffer + 1;

    ck_assert_int_eq(value_map_copy(&map, &MAP, &data, &size), 0);
    ck_assert_value_map_eq(&map, &MAP);
}
END_TEST

/*----------------------------------------------------------------------------*
 |                            rbh_value_validate()                            |
 *----------------------------------------------------------------------------*/

START_TEST(rvv_bad_type)
{
    const struct rbh_value INVALID = {
        .type = -1,
    };

    errno = 0;
    ck_assert_int_eq(rbh_value_validate(&INVALID), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rvv_binary_empty)
{
    const struct rbh_value BINARY = {
        .type = RBH_VT_BINARY,
        .binary = {
            .size = 0,
        },
    };

    ck_assert_int_eq(rbh_value_validate(&BINARY), 0);
}
END_TEST

START_TEST(rvv_binary_nonempty)
{
    const struct rbh_value BINARY = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = "abcdefg",
            .size = 8,
        },
    };

    ck_assert_int_eq(rbh_value_validate(&BINARY), 0);
}
END_TEST

START_TEST(rvv_binary_nonempty_null)
{
    const struct rbh_value BINARY = {
        .type = RBH_VT_BINARY,
        .binary = {
            .data = NULL,
            .size = 1,
        },
    };

    errno = 0;
    ck_assert_int_eq(rbh_value_validate(&BINARY), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rvv_uint32)
{
    const struct rbh_value UINT32 = {
        .type = RBH_VT_UINT32,
    };

    ck_assert_int_eq(rbh_value_validate(&UINT32), 0);
}
END_TEST

START_TEST(rvv_uint64)
{
    const struct rbh_value UINT64 = {
        .type = RBH_VT_UINT64,
    };

    ck_assert_int_eq(rbh_value_validate(&UINT64), 0);
}
END_TEST

START_TEST(rvv_int32)
{
    const struct rbh_value INT32 = {
        .type = RBH_VT_INT32,
    };

    ck_assert_int_eq(rbh_value_validate(&INT32), 0);
}
END_TEST

START_TEST(rvv_int64)
{
    const struct rbh_value INT64 = {
        .type = RBH_VT_INT64,
    };

    ck_assert_int_eq(rbh_value_validate(&INT64), 0);
}
END_TEST

START_TEST(rvv_string_null)
{
    const struct rbh_value STRING = {
        .type = RBH_VT_STRING,
        .string = NULL,
    };

    errno = 0;
    ck_assert_int_eq(rbh_value_validate(&STRING), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rvv_string_nonnull)
{
    const struct rbh_value STRING = {
        .type = RBH_VT_STRING,
        .string = "abcdefg",
    };

    ck_assert_int_eq(rbh_value_validate(&STRING), 0);
}
END_TEST

START_TEST(rvv_regex_null)
{
    const struct rbh_value REGEX = {
        .type = RBH_VT_REGEX,
        .regex = {
            .string = NULL,
            .options = 0,
        },
    };

    errno = 0;
    ck_assert_int_eq(rbh_value_validate(&REGEX), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rvv_regex_bad_option)
{
    const struct rbh_value REGEX = {
        .type = RBH_VT_REGEX,
        .regex = {
            .string = "abcdefg",
            .options = RBH_RO_ALL + 1,
        },
    };

    errno = 0;
    ck_assert_int_eq(rbh_value_validate(&REGEX), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rvv_regex_valid)
{
    const struct rbh_value REGEX = {
        .type = RBH_VT_REGEX,
        .regex = {
            .string = "abcdefg",
            .options = RBH_RO_ALL,
        },
    };

    ck_assert_int_eq(rbh_value_validate(&REGEX), 0);
}
END_TEST

START_TEST(rvv_sequence_empty)
{
    const struct rbh_value SEQUENCE = {
        .type = RBH_VT_SEQUENCE,
        .sequence = {
            .count = 0,
        },
    };

    ck_assert_int_eq(rbh_value_validate(&SEQUENCE), 0);
}
END_TEST

START_TEST(rvv_sequence_nonempty_null)
{
    const struct rbh_value SEQUENCE = {
        .type = RBH_VT_SEQUENCE,
        .sequence = {
            .values = NULL,
            .count = 1,
        },
    };

    errno = 0;
    ck_assert_int_eq(rbh_value_validate(&SEQUENCE), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rvv_sequence_with_invalid_value)
{
    const struct rbh_value INVALID = {
        .type = -1,
    };
    const struct rbh_value SEQUENCE = {
        .type = RBH_VT_SEQUENCE,
        .sequence = {
            .values = &INVALID,
            .count = 1,
        },
    };

    errno = 0;
    ck_assert_int_eq(rbh_value_validate(&SEQUENCE), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rvv_sequence_with_valid_value)
{
    const struct rbh_value VALID = {
        .type = RBH_VT_INT32,
    };
    const struct rbh_value SEQUENCE = {
        .type = RBH_VT_SEQUENCE,
        .sequence = {
            .values = &VALID,
            .count = 1,
        },
    };

    ck_assert_int_eq(rbh_value_validate(&SEQUENCE), 0);
}
END_TEST

START_TEST(rvv_map_empty)
{
    const struct rbh_value MAP = {
        .type = RBH_VT_MAP,
        .map = {
            .count = 0,
        },
    };

    ck_assert_int_eq(rbh_value_validate(&MAP), 0);
}
END_TEST

START_TEST(rvv_map_nonempty_null)
{
    const struct rbh_value MAP = {
        .type = RBH_VT_MAP,
        .map = {
            .pairs = NULL,
            .count = 1,
        },
    };

    errno = 0;
    ck_assert_int_eq(rbh_value_validate(&MAP), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rvv_map_with_null_key)
{
    const struct rbh_value VALUE = {
        .type = RBH_VT_INT32,
    };
    const struct rbh_value_pair PAIR = {
        .key = NULL,
        .value = &VALUE,
    };
    const struct rbh_value MAP = {
        .type = RBH_VT_MAP,
        .map = {
            .pairs = &PAIR,
            .count = 1,
        },
    };

    errno = 0;
    ck_assert_int_eq(rbh_value_validate(&MAP), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rvv_map_with_null_value)
{
    const struct rbh_value_pair PAIR = {
        .key = "abcdefg",
        .value = NULL,
    };
    const struct rbh_value MAP = {
        .type = RBH_VT_MAP,
        .map = {
            .pairs = &PAIR,
            .count = 1,
        },
    };

    errno = 0;
    ck_assert_int_eq(rbh_value_validate(&MAP), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rvv_map_with_invalid_value)
{
    const struct rbh_value INVALID = {
        .type = -1,
    };
    const struct rbh_value_pair PAIR = {
        .key = "abcdefg",
        .value = &INVALID,
    };
    const struct rbh_value MAP = {
        .type = RBH_VT_MAP,
        .map = {
            .pairs = &PAIR,
            .count = 1,
        },
    };

    errno = 0;
    ck_assert_int_eq(rbh_value_validate(&MAP), -1);
    ck_assert_int_eq(errno, EINVAL);
}
END_TEST

START_TEST(rvv_map_with_valid_value)
{
    const struct rbh_value VALID = {
        .type = RBH_VT_INT32,
    };
    const struct rbh_value_pair PAIR = {
        .key = "abcdefg",
        .value = &VALID,
    };
    const struct rbh_value MAP = {
        .type = RBH_VT_MAP,
        .map = {
            .pairs = &PAIR,
            .count = 1,
        },
    };

    ck_assert_int_eq(rbh_value_validate(&MAP), 0);
}
END_TEST

/* TODO: test value_copy() and value_data_size() */

static Suite *
unit_suite(void)
{
    Suite *suite;
    TCase *tests;

    suite = suite_create("value");

    tests = tcase_create("rbh_value_boolean_new");
    tcase_add_test(tests, rvbn_false);
    tcase_add_test(tests, rvbn_true);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_value_int32_new");
    tcase_add_test(tests, rvi32n_min);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_value_uint32_new");
    tcase_add_test(tests, rvu32n_max);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_value_int64_new");
    tcase_add_test(tests, rvi64n_min);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_value_uint64_new");
    tcase_add_test(tests, rvu64n_max);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_value_string_new");
    tcase_add_test(tests, rvstrn_basic);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_value_binary_new");
    tcase_add_test(tests, rvbn_basic);
    tcase_add_test(tests, rvbn_empty);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_value_regex_new");
    tcase_add_test(tests, rvrn_basic);
    tcase_add_test(tests, rvrn_bad_option);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_value_sequence_new");
    tcase_add_test(tests, rvseqn_basic);
    tcase_add_test(tests, rvseqn_empty);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_value_map_new");
    tcase_add_test(tests, rvmn_basic);
    tcase_add_test(tests, rvmn_empty);
    tcase_add_test(tests, rvmn_misaligned);

    suite_add_tcase(suite, tests);

    tests = tcase_create("value_map_data_size");
    tcase_add_test(tests, vmds_bad_type);
    tcase_add_test(tests, vmds_bad_type_in_sequence);

    suite_add_tcase(suite, tests);

    tests = tcase_create("value_map_copy");
    tcase_add_test(tests, vmc_too_small_for_array_of_pairs);
    tcase_add_test(tests, vmc_too_small_for_key);
    tcase_add_test(tests, vmc_too_small_for_bare_value);
    tcase_add_test(tests, vmc_bad_type);
    tcase_add_test(tests, vmc_too_small_for_binary_value);
    tcase_add_test(tests, vmc_too_small_for_string_value);
    tcase_add_test(tests, vmc_too_small_for_regex_value);
    tcase_add_test(tests, vmc_too_small_for_bare_sequence_values);
    tcase_add_test(tests, vmc_too_small_for_sequence_values);
    tcase_add_test(tests, vmc_too_small_for_map_value);
    tcase_add_test(tests, vmc_misaligned_buffer);

    suite_add_tcase(suite, tests);

    tests = tcase_create("rbh_value_validate");
    tcase_add_test(tests, rvv_bad_type);
    tcase_add_test(tests, rvv_binary_empty);
    tcase_add_test(tests, rvv_binary_nonempty);
    tcase_add_test(tests, rvv_binary_nonempty_null);
    tcase_add_test(tests, rvv_uint32);
    tcase_add_test(tests, rvv_uint64);
    tcase_add_test(tests, rvv_int32);
    tcase_add_test(tests, rvv_int64);
    tcase_add_test(tests, rvv_string_null);
    tcase_add_test(tests, rvv_string_nonnull);
    tcase_add_test(tests, rvv_regex_null);
    tcase_add_test(tests, rvv_regex_bad_option);
    tcase_add_test(tests, rvv_regex_valid);
    tcase_add_test(tests, rvv_sequence_empty);
    tcase_add_test(tests, rvv_sequence_nonempty_null);
    tcase_add_test(tests, rvv_sequence_with_invalid_value);
    tcase_add_test(tests, rvv_sequence_with_valid_value);
    tcase_add_test(tests, rvv_map_empty);
    tcase_add_test(tests, rvv_map_nonempty_null);
    tcase_add_test(tests, rvv_map_with_null_key);
    tcase_add_test(tests, rvv_map_with_null_value);
    tcase_add_test(tests, rvv_map_with_invalid_value);
    tcase_add_test(tests, rvv_map_with_valid_value);

    suite_add_tcase(suite, tests);

    return suite;
}

int
main(void)
{
    int number_failed;
    Suite *suite;
    SRunner *runner;

    suite = unit_suite();
    runner = srunner_create(suite);

    srunner_run_all(runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
