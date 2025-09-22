/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef DEDUPLICATOR_H
#define DEDUPLICATOR_H

#include <stddef.h>

#include <robinhood/iterator.h>

#include "source.h"

struct rbh_mut_iterator *
deduplicator_new(size_t batch_size, struct source *source, size_t nb_workers);

struct dedup_iter {
    struct rbh_iterator *iter;
    size_t index;
};

#endif
