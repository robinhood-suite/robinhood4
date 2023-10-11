/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef DEDUPLICATOR_H
#define DEDUPLICATOR_H

#include <stddef.h>

#include <robinhood/iterator.h>

#include "source.h"

struct rbh_mut_iterator *
deduplicator_new(size_t batch_size, size_t flush_size, struct source *source);

#endif
