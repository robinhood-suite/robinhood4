/* SPDX-License-Identifer: LGPL-3.0-or-later */

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
source_from_lustre_changelog(const char *mdtname);

#endif
