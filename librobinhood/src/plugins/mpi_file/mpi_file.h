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

/**
 * The structure of the rbh_filter is reproduced with mfu_pred structure.
 * Like the rbh_filter, there are logical mfu_pred and comparison mfu_pred.
 * Each mfu_pred takes a function and an argument. The function determines if
 * it's a logical or comparison mfu_pred.
 *
 * Logical mfu_pred:
 *
 * Logical mfu_pred are combinations of other mfu_pred.
 *
 * The logical mfu_pred functions are _MFU_PRED_AND, _MFU_PRED_OR, _MFU_PRED_NOT
 * and_MFU_PRED_NULL.
 *
 * It takes as an argument a mfu_pred which is the first element of a linked
 * list of mfu_pred.
 *
 * Comparison mfu_pred:
 *
 * The comparison mfu_pred represents a single predicate.
 *
 * The comparison mfu_pred functions are _MFU_PRED_SIZE, _MFU_PRED_PATH,...,
 * and all the functions from mpiFileUtils.
 *
 * It takes as an argument the predicate's value.
 *
 * Example:
 *  -name file -and -type f
 *  <=>
 *               mfu_pred
 *  function: _MFU_PRED_AND
 *                  |             next
 *  argument:    mfu_pred ---------------------> mfu_pred
                    |                                |
 *  function: MFU_PRED_NAME          function: MFU_PRED_TYPE
 *                  |                                |
 *  argument:     value               argument:    value
 *                "file"                            "f"
 *
 *  -not -type f
 *  <=>
 *               mfu_pred
 *  function: _MFU_PRED_NOT
 *                  |
 *  argument:    mfu_pred
 *                  |
 *  function: MFU_PRED_TYPE
 *                  |
 *  argument:     value
 *                 "f"
 */


/*----------------------------------------------------------------------------*
 |                                Backend Functions                           |
 *----------------------------------------------------------------------------*/

void
rbh_mpi_file_plugin_destroy(void);


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
 |                          PRED LOGICAL FUNCTION                             |
 *---------------------------------------------------------------------------*/

int
_MFU_PRED_AND(mfu_flist flist, uint64_t idx, void *arg);

int
_MFU_PRED_NULL(mfu_flist flist, uint64_t idx, void *arg);

int
_MFU_PRED_NOT(mfu_flist flist, uint64_t idx, void *arg);

int
_MFU_PRED_OR(mfu_flist flist, uint64_t idx, void *arg);

/*----------------------------------------------------------------------------*
 |                                  FILTER                                    |
 *----------------------------------------------------------------------------*/

bool
convert_rbh_filter(mfu_pred *pred, mfu_pred_times *now, int prefix_len,
                   const struct rbh_filter *filter);

mfu_pred *
rbh_filter2mfu_pred(const struct rbh_filter *filter, int prefix_len,
                    mfu_pred_times *now);

void
_mfu_pred_free(mfu_pred *pred);

#endif
