/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <error.h>

#include <robinhood/utils.h>

#include "lustre_utils.h"

/** How the batch save/ack works:
 *
 *  When we send a batch for enrichment, we call the lustre_changelog_save_batch
 *  function to record the last changelog index for that batch. When a thread
 *  finishes enriching its sub-batch, it calls the lustre_changelog_ack_batch
 *  function, which decrements the ack_required counter. If ack_required reaches
 *  0, it means that all threads have finished enriching the batch, and the
 *  changelogs for that batch can be acknowledged.
 *
 *  However, there’s a trickier case where a newer batch might finish before an
 *  older one. In that case, we need to check if there is no older batches that
 *  aren't done before acknowledging the changelog.
 */
void lustre_changelog_save_batch(void *source, size_t ack_required,
                                        bool dedup)
{
    struct lustre_changelog_iterator *events;
    struct lustre_source *lustre = source;
    struct source_batch_node *new_node;

    events = &lustre->events;

    if (events->username == NULL)
        return;

    /** Without deduplication, each changelog generates fsevents, and each
     *  fsevent is treated as a batch. However, we only want to acknowledge the
     *  changelog once the last fsevent/batch has been processed.
     *
     *  To achieve this, we update the batch saved for this changelog so that it
     *  matches the batch_id of the last fsevent/batch returned for that
     *  changelog. This way, only the acknowledgment from the final
     *  fsevent/batch associated with this changelog will actually acknowledge
     *  the changelog itself.
     */
    if (!dedup &&
        events->last_batch_changelog_index == events->last_changelog_index) {
        events->curr_batch->batch_id++;
        lustre->batch_id++;
        return;
    }

    new_node = xmalloc(sizeof(*new_node));
    new_node->batch_id = lustre->batch_id;
    /* We need to do last_changelog_index - 1 because with the dedup we always
     * try to read the next changelog until the batch is full. If the batch is
     * full, the last changelog is kept in memory.
     */
    if (dedup && !events->empty)
        new_node->last_changelog_index = events->last_changelog_index - 1;
    else
        new_node->last_changelog_index = events->last_changelog_index;

    new_node->ack_required = ack_required;

    pthread_mutex_lock(&lustre->batch_lock);
    rbh_list_add_tail(lustre->batch_list, &new_node->link);
    pthread_mutex_unlock(&lustre->batch_lock);

    lustre->batch_id++;
    events->last_batch_changelog_index = events->last_changelog_index;
    events->curr_batch = new_node;
}

void lustre_changelog_ack_batch(void *source, uint64_t batch_id)
{
    struct lustre_source *lustre = source;
    struct source_batch_node *elem, *tmp;
    bool can_clear = true;
    int rc;

    if (lustre->events.username == NULL)
        return;

    pthread_mutex_lock(&lustre->batch_lock);

    /** We decrement the ack_required inside the batch and try to acknowledge
     *  the changelog for this batch.
     *
     *  Also, we check that no older batch are not finished. If so, we can't
     *  acknowledge the changelog for this batch.
     *
     *  Finally, we try to ack the changelog of all the batch ready to be ack.
     */
    rbh_list_foreach_safe(lustre->batch_list, elem, tmp, link) {
        /* If a batch older than this batch is not yet finished, we can't
         * continue to ack the changelog
         */
        if (elem->ack_required > 0 && elem->batch_id < batch_id)
            can_clear = false;

        if (elem->batch_id > batch_id && elem->ack_required > 0)
            break;

        if (elem->batch_id == batch_id)
            elem->ack_required--;

        /* We ack the changelog only if no older batch not yet finished has
         * been found
         */
        if (elem->ack_required == 0 && can_clear) {
            rbh_list_del(&elem->link);
            rc = llapi_changelog_clear(lustre->events.mdt_name,
                                       lustre->events.username,
                                       elem->last_changelog_index);
            free(elem);
            if (rc < 0)
                error(EXIT_FAILURE, errno, "llapi_changelog_clear");
        }
    }

    pthread_mutex_unlock(&lustre->batch_lock);
}
