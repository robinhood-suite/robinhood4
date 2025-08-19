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
#include <robinhood/hashmap.h>

#include "source.h"

struct rbh_mut_iterator *
deduplicator_new(size_t batch_size, struct source *source,
                 struct rbh_hashmap *pool_in_process,
                 pthread_mutex_t *pool_mutex, int *avail_batches);

#endif
