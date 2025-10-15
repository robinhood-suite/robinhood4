/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>

#include <lustre/lustreapi.h>
#include <linux/lustre/lustre_fid.h>

#include <robinhood/utils.h>

#include "lustre.h"

#include "source.h"
#include "utils.h"

static const struct rbh_iterator_operations LUSTRE_CHANGELOG_ITER_OPS = {
    .next = lustre_changelog_iter_next,
    .destroy = lustre_changelog_iter_destroy,
};

static const struct rbh_iterator LUSTRE_CHANGELOG_ITERATOR = {
    .ops = &LUSTRE_CHANGELOG_ITER_OPS,
};

static uint64_t
lustre_changelog_get_start_idx(struct lustre_changelog_iterator *events,
                               const char *mdtname)
{
    const struct rbh_value *mdts, *mdt, *last_read;
    const struct rbh_value_pair *pair;
    struct rbh_value_map *info_map;

    info_map = sink_get_info(events->sink, RBH_INFO_FSEVENTS_SOURCE);
    if (info_map == NULL)
        return 0;

    assert(info_map->count == 1);

    pair = &info_map->pairs[0];
    mdts = pair->value;

    assert(mdts->type == RBH_VT_MAP);

    mdt = rbh_map_find(&mdts->map, mdtname);
    if (mdt == NULL)
        return 0;

    last_read = rbh_map_find(&mdt->map, "last_read");
    if (last_read == NULL)
        return 0;

    return last_read->uint64;
}

static void
lustre_changelog_iter_init(struct lustre_changelog_iterator *events,
                           const char *mdtname, const char *username,
                           const char *dump_file, uint64_t max_changelog,
                           struct sink *sink)
{
    const char *mdtname_index;
    uint64_t start_index = 0;
    int rc;

    events->max_changelog = max_changelog;
    events->nb_changelog = 0;
    events->sink = sink;
    events->empty = false;

    start_index = lustre_changelog_get_start_idx(events, mdtname);
    events->last_changelog_index = start_index;
    events->last_batch_changelog_index = start_index;

    rc = llapi_changelog_start(&events->reader,
                               CHANGELOG_FLAG_JOBID |
                               CHANGELOG_FLAG_EXTRA_FLAGS,
                               mdtname, start_index == 0 ? 0 : start_index + 1);
    if (rc < 0)
        error(EXIT_FAILURE, -rc, "llapi_changelog_start");

    rc = llapi_changelog_set_xflags(events->reader,
                                    CHANGELOG_EXTRA_FLAG_UIDGID |
                                    CHANGELOG_EXTRA_FLAG_NID |
                                    CHANGELOG_EXTRA_FLAG_OMODE |
                                    CHANGELOG_EXTRA_FLAG_XATTR);
    if (rc < 0)
        error(EXIT_FAILURE, -rc, "llapi_changelog_set_xflags");

    events->iterator = LUSTRE_CHANGELOG_ITERATOR;
    events->fsevents_iterator = NULL;

    events->username = xstrdup_safe(username);
    events->mdt_name = xstrdup(mdtname);

    for (mdtname_index = mdtname + strlen(mdtname) - 1;
         isdigit(*mdtname_index); mdtname_index--);

    rc = str2int64_t(++mdtname_index, (int64_t *) &events->source_mdt_index);
    if (rc)
        error(EXIT_FAILURE, errno, "str2int64_t");

    if (dump_file == NULL) {
        events->dump_file = NULL;
    } else if (strcmp(dump_file, "-") == 0) {
        events->dump_file = stdout;
    } else {
        events->dump_file = fopen(dump_file, "a");
        if (events->dump_file == NULL)
            error(EXIT_FAILURE, errno, "Failed to open the dump file");
    }
}

static const void *
source_iter_next(void *iterator)
{
    struct lustre_source *source = iterator;

    return rbh_iter_next(&source->events.iterator);
}

static void
source_iter_destroy(void *iterator)
{
    struct lustre_source *source = iterator;

    rbh_list_del(source->batch_list);
    pthread_mutex_destroy(&source->batch_lock);
    free(source->batch_list);
    rbh_iter_destroy(&source->events.iterator);
    free(source);
}

static const struct rbh_iterator_operations SOURCE_ITER_OPS = {
    .next = source_iter_next,
    .destroy = source_iter_destroy,
};

static const struct source LUSTRE_SOURCE = {
    .name = "lustre",
    .fsevents = {
        .ops = &SOURCE_ITER_OPS,
    },
    .save_batch = lustre_changelog_save_batch,
    .ack_batch = lustre_changelog_ack_batch,
};

struct source *
source_from_lustre_changelog(const char *mdtname, const char *username,
                             const char *dump_file, uint64_t max_changelog,
                             struct sink *sink)
{
    struct lustre_source *source;

    source = xmalloc(sizeof(*source));

    lustre_changelog_iter_init(&source->events, mdtname, username,
                               dump_file, max_changelog, sink);

    initialize_source_stack(sizeof(struct rbh_value_pair) * (1 << 7));
    source->batch_list = xmalloc(sizeof(*source->batch_list));
    rbh_list_init(source->batch_list);
    source->batch_id = 1;
    pthread_mutex_init(&source->batch_lock, NULL);
    source->source = LUSTRE_SOURCE;

    return &source->source;
}
