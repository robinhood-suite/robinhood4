/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_S3_INTERNALS_H
#define ROBINHOOD_S3_INTERNALS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <robinhood/backend.h>

char *
get_next_object(struct s3_iterator *s3_iter);

int
fill_path(char *path, struct rbh_value_pair **_pairs,
          struct rbh_sstack *values);

void
fill_user_metadata(struct rbh_value_pair *pairs,
                   struct rbh_sstack *values);

void *
s3_iter_next(void *iterator);

#endif
