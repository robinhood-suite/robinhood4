/* This file is part of the RobinHood Library
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "robinhood/utils.h"
#include "robinhood/value.h"
#include "robinhood/sstack.h"

static int
convert_pair_with_op(struct rbh_value_pair *pair, const char *op,
                     struct rbh_sstack *stack)
{
    const struct rbh_value *xattr_value = pair->value;
    struct rbh_value_pair *pair_op;
    struct rbh_value *value;

    pair_op = RBH_SSTACK_PUSH(stack, NULL, sizeof(*pair_op));
    pair_op->key = op;
    pair_op->value = xattr_value;

    value = RBH_SSTACK_PUSH(stack, NULL, sizeof(*value));
    value->type = RBH_VT_MAP;
    value->map.count = 1;
    value->map.pairs = pair_op;

    pair->value = value;

    return 0;
}

int
convert_xattrs_with_operation(const struct rbh_value_pair *pairs, int count,
                              const char *op, struct rbh_sstack *stack)
{
    for (int i = 0; i < count; i++) {
        struct rbh_value_pair *pair = (struct rbh_value_pair *) &pairs[i];

        if (convert_pair_with_op(pair, op, stack))
            return -1;
    }

    return 0;
}
