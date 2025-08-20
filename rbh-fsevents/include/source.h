/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_FSEVENTS_SOURCE_H
#define RBH_FSEVENTS_SOURCE_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <robinhood/iterator.h>

struct source {
    struct rbh_iterator fsevents;
    const char *name;
};

struct source *
source_from_file(FILE *file);

struct source *
source_from_lustre_changelog(const char *mdtname, const char *username,
                             const char *dump_file, uint64_t max_changelog);

struct source *
source_from_hestia_file(FILE *file);

#endif
