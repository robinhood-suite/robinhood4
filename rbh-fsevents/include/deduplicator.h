/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef DEDUPLICATOR_H
#define DEDUPLICATOR_H

#include <stddef.h>

#include <robinhood/iterator.h>

#include "source.h"

struct rbh_mut_iterator *
deduplicator_new(size_t count, struct source *source);

#endif
