/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/xattr.h>

#include "sparse_internals.h"
#include "robinhood/statx.h"
#include "robinhood/utils.h"
#include "robinhood/backends/common.h"
#include "robinhood/backends/sparse.h"
#include "sstack.h"
#include "value.h"

int
rbh_sparse_enrich(struct entry_info *einfo, uint64_t flags,
                  struct rbh_value_pair *pairs, size_t pairs_count,
                  struct rbh_sstack *values)
{
    struct rbh_statx *statx = einfo->statx;
    int sparseness;

    if (!rbh_attr_is_sparse(flags))
        return 0;

    if (!S_ISREG(statx->stx_mode))
        return 0;

    if (statx->stx_size)
        sparseness = (512.0 * statx->stx_blocks / statx->stx_size) * 100;
    else
        /* GNU's find considers that empty files have an undefined sparseness,
         * but actually prints them as non-sparse when requested. So we choose
         * this logic and record empty files as non-sparse aswell.
         */
        sparseness = 100;

    if (fill_uint32_pair("sparseness", sparseness, &pairs[0], values))
        return -1;

    return 1;
}

int
rbh_sparse_setup(void)
{
    // probably to init config
    return 0;
}

void
rbh_sparse_helper(__attribute__((unused)) const char *backend,
                  __attribute__((unused)) struct rbh_config *config,
                  char **predicate_helper, char **directive_helper)
{
    int rc;

    rc = asprintf(predicate_helper,
        " - Sparse:\n"
        "   -sparse              filter entries based on if they are sparse.\n"
        "                        A file is considered sparse if\n"
        "                        512 * statx->st_blocks / statx->st_size < 1.\n"
        "                        Empty files are considered non-sparse.\n");

    if (rc == -1)
        *predicate_helper = NULL;

    rc = asprintf(directive_helper,
        " - Sparse:\n"
        "   %%S         File's sparseness. Printed as 512 * st_blocks / st_size.\n"
        "               A file is considered sparse if the result is less than 1.\n"
        "               Empty files are considered non-sparse.\n");

    if (rc == -1)
        *directive_helper = NULL;
}
