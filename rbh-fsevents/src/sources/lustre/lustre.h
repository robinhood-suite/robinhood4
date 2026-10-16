/* This file is part of RobinHood
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_FSEVENTS_SOURCE_LUSTRE_H
#define RBH_FSEVENTS_SOURCE_LUSTRE_H

#include <stdbool.h>
#include <pthread.h>

#include <lustre/lustreapi.h>

#include <robinhood/list.h>
#include <robinhood/iterator.h>

#include "source.h"

/** Use the changelog enum from Lustre to determine the number of changelogs
 *  we should handle.
 *
 *  At the time of writing this, CL_LAST = 25.
 */
struct lustre_source_md {
    size_t changelog_count[CL_LAST];
};

int
print_lustre_source_md(struct rbh_fsevents_metadata *fsevents_md,
                       char *message, size_t *message_avail_size);

struct rbh_value_map *
convert_lustre_source_md(struct rbh_fsevents_metadata *fsevents_md);

/** This structure will contains all the information of one batch sent to
 *  enrichment. It contains the last changelog index read for that batch,
 *  the number of threads the batch was divided among
 */
struct source_batch_node {
    struct rbh_list_node link;
    uint64_t batch_id;             /** Id of this batch */
    uint64_t last_changelog_index; /** Index of the last changelog index read */
    size_t ack_required;           /** Number of threads the batch was divided
                                    *  among
                                    */
};

struct lustre_changelog_iterator {
    struct rbh_iterator iterator;

    void *reader;
    struct sink *sink;
    struct rbh_iterator *fsevents_iterator;

    char *username;
    char *mdt_name;
    int32_t source_mdt_index;
    uint64_t last_changelog_index;
    uint64_t last_batch_changelog_index;
    uint64_t max_changelog;
    bool empty;

    /* Only use without dedup, it's a reference to the current batch in the
     * list of batches saved to avoid iterating over the list each times.
     */
    struct source_batch_node *curr_batch;

    FILE *dump_file;

    struct rbh_fsevents_metadata *fsevents_md;
};

struct lustre_source {
    struct source source;

    /** List of all the batch sent to enrichment */
    struct rbh_list_node *batch_list;
    pthread_mutex_t batch_lock;
    /** Current batch id */
    uint64_t batch_id;
    struct lustre_changelog_iterator events;
};

void lustre_changelog_save_batch(void *source, size_t ack_required,
                                 bool dedup);

void lustre_changelog_ack_batch(void *source, uint64_t batch_id,
                                struct sink *sink);

const void *
lustre_changelog_iter_next(void *iterator);

void
lustre_changelog_iter_destroy(void *iterator);

#endif
