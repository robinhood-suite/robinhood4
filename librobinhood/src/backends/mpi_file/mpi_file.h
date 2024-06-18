/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_MPI_FILE_H
#define RBH_MPI_FILE_H

#include <stdbool.h>

#include "mfu.h"
#include "robinhood/filter.h"

/*----------------------------------------------------------------------------*
 |                                PRED FUNCTION                               |
 *----------------------------------------------------------------------------*/

mfu_pred_times_rel *
_mfu_pred_relative(const struct rbh_filter *filter, const mfu_pred_times *now);

int
_MFU_PRED_SIZE(mfu_flist flist, uint64_t idx, void *arg);

int
_MFU_PRED_PATH(mfu_flist flist, uint64_t idx, void *arg);

/*----------------------------------------------------------------------------*
 |                                  FILTER                                    |
 *----------------------------------------------------------------------------*/
bool
convert_rbh_filter(mfu_pred *pred, mfu_pred_times *now, int prefix_len,
                   const struct rbh_filter *filter);

mfu_pred *
rbh_filter2mfu_pred(const struct rbh_filter *filter, int prefix_len,
                    mfu_pred_times *now);
#endif
