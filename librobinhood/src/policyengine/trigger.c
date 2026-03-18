/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <errno.h>
#include <stdint.h>

#include "robinhood/policyengine/trigger.h"
#include <robinhood.h>

static int
extract_int(const struct rbh_value *value, int64_t *out)
{
    switch (value->type) {
    case RBH_VT_INT32:
        *out = (int64_t)value->int32;
        return 0;
    case RBH_VT_INT64:
        *out = value->int64;
        return 0;
    default:
        errno = EINVAL;
        return -1;
    }
}

int
rbh_trigger_query_stat(struct rbh_backend *backend,
                       const struct rbh_filter *filter,
                       uint32_t stat_field, int64_t *result)
{
    struct rbh_accumulator_field acc_field = {0};
    struct rbh_filter_options options = {0};
    const struct rbh_value_map *output_map;
    struct rbh_filter_output output = {0};
    struct rbh_group_fields group = {0};
    struct rbh_mut_iterator *iter;
    struct rbh_value_map *map;

    if (stat_field == 0) {
        acc_field.accumulator = FA_COUNT;
    } else {
        acc_field.accumulator = FA_SUM;
        acc_field.field.fsentry = RBH_FP_STATX;
        acc_field.field.statx = stat_field;
    }

    group.acc_fields = &acc_field;
    group.acc_count = 1;

    output.type = RBH_FOT_VALUES;
    output.output_fields.fields = &acc_field;
    output.output_fields.count = 1;

    iter = rbh_backend_report(backend, filter, &group, &options, &output);
    if (iter == NULL)
        return -1;

    map = rbh_mut_iter_next(iter);
    if (map == NULL) {
        rbh_mut_iter_destroy(iter);
        if (errno == ENODATA) {
            *result = 0;
            return 0;
        }
        return -1;
    }

    if (map->count != 1) {
        rbh_mut_iter_destroy(iter);
        errno = EINVAL;
        return -1;
    }

    output_map = &map->pairs[0].value->map;
    if (output_map->count != 1) {
        rbh_mut_iter_destroy(iter);
        errno = EINVAL;
        return -1;
    }

    if (extract_int(output_map->pairs[0].value, result) == -1) {
        rbh_mut_iter_destroy(iter);
        return -1;
    }

    rbh_mut_iter_destroy(iter);
    return 0;
}
