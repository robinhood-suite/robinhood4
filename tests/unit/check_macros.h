/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifndef ROBINHOOD_CHECK_MACROS_H
#define ROBINHOOD_CHECK_MACROS_H

#include "check-compat.h"

#define ck_assert_id_eq(X, Y) do { \
    ck_assert_uint_eq((X)->size, (Y)->size); \
    if ((X)->size != 0) \
        ck_assert_mem_eq((X)->data, (Y)->data, (X)->size); \
} while (0)

#endif
