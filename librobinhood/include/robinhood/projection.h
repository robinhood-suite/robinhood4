/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_PROJECTION_H
#define ROBINHOOD_PROJECTION_H

#include "robinhood/backend.h"
#include "robinhood/statx.h"

/**
 * Add a new field in the projection
 *
 * @param projection   the projection to used
 * @param field        the field to retrieved
 */
void
rbh_projection_add(struct rbh_filter_projection *projection,
                   const struct rbh_filter_field *field);

/**
 * Remove a field from the projection
 *
 * @param projection   the projection to used
 * @param field        the field to removed
 */
void
rbh_projection_remove(struct rbh_filter_projection *projection,
                      const struct rbh_filter_field *field);

/**
 * Set a field in the projection
 *
 * @param projection   the projection to used
 * @param field        the field to set
 */
void
rbh_projection_set(struct rbh_filter_projection *projection,
                   const struct rbh_filter_field *field);

#endif
