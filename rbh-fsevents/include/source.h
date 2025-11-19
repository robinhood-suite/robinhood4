/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_FSEVENTS_SOURCE_H
#define RBH_FSEVENTS_SOURCE_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "sink.h"
#include <robinhood/iterator.h>

struct source {
    struct rbh_iterator fsevents;
    const char *name;

    /** Callback to save all the information (cf. changelogs for Lustre) the
     * source has read to create a batch. The information saved will be used
     * by the ack_batch callback.
     */
    void (*save_batch)(void *source, size_t ack_required, bool dedup);

    /** Callback to acknowledge a batch to the source. It will be used to free
     *  the memory associated with this batch in the source (cf. ack the
     *  changelogs for Lustre).
     */
    void (*ack_batch)(void *source, uint64_t batch_id);
};

struct source *
source_from_file(FILE *file);

struct source *
source_from_lustre_changelog(const char *mdtname, const char *username,
                             const char *dump_file, uint64_t max_changelog,
                             struct sink *sink);

struct source *
source_from_hestia_file(FILE *file);

#endif
