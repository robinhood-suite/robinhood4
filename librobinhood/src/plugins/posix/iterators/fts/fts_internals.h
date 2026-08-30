/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_POSIX_FTS_INTERNALS_H
#define ROBINHOOD_POSIX_FTS_INTERNALS_H

#include <robinhood/iterator.h>
#include <robinhood/log.h>

struct rbh_mut_iterator *
fts_iter_new(struct rbh_metadata *metadata, const char *root, const char *entry,
             int statx_sync_type, bool one);

#endif
