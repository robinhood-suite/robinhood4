/* This file is part of rbh-fsevents
 * Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stddef.h>
#include <robinhood/id.h>

size_t
hash_id(const struct rbh_id *id);
