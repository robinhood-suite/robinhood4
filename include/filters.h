/* This file is part of rbh-find-lustre
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_FIND_LUSTRE_FILTERS_H
#define RBH_FIND_LUSTRE_FILTERS_H

#include "parser.h"

#include <rbh-find/filters.h>

/**
 * Placeholder function for future fields to filter.
 *
 * @param placeholder_field  placeholder field name
 *
 * @return             a pointer to a newly allocated struct rbh_filter
 *
 * Exit on error
 */
struct rbh_filter *
placeholder2filter(const char *placeholder_field);

#endif
