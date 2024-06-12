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

bool
convert_rbh_filter(mfu_pred *pred, mfu_pred_times *now, int prefix_len,
                   const struct rbh_filter *filter);

mfu_pred *
rbh_filter2mfu_pred(const struct rbh_filter *filter, int prefix_len,
                    mfu_pred_times *now);
#endif
