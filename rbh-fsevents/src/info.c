/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <linux/limits.h>
#include <stdlib.h>

#include <robinhood/utils.h>

#include "info.h"

int
insert_backend_source(char *cmd_backend,
                      struct enrich_iter_builder *enrich_builder,
                      struct sink *sink)
{
    struct rbh_value command_backend_value = {
        .type = RBH_VT_STRING,
        .string = cmd_backend
    };
    struct rbh_value_map *info_map;
    struct rbh_value_map sync_map;
    struct rbh_value_pair pair;

    info_map = enrich_iter_builder_get_source_backends(enrich_builder);
    if (info_map == NULL) {
        fprintf(stderr, "Failed to retrieve source backends from enricher\n");
        return -1;
    }

    if (sink_insert_info(sink, info_map)) {
        fprintf(stderr, "Failed to set backend_info\n");
        return -1;
    }

    pair.key = "command_backend";
    pair.value = &command_backend_value;

    sync_map.pairs = &pair;
    sync_map.count = 1;

    if (sink_insert_info(sink, &sync_map)) {
        fprintf(stderr, "Failed to set command_backend\n");
        return -1;
    }

    return 0;
}

int
insert_mountpoint(struct enrich_iter_builder *enrich_builder,
                  struct sink *sink, const char **enrich_mountpoint)
{
    struct rbh_value_map mountpoint_map;
    struct rbh_value mountpoint;
    struct rbh_value_pair pair;
    char clean_path[PATH_MAX];
    size_t len;

    strncpy(clean_path, enrich_builder->mount_path, sizeof(clean_path));
    clean_path[sizeof(clean_path) - 1] = '\0';

    len = strlen(clean_path);
    if (len > 1 && clean_path[len - 1] == '/')
        clean_path[len - 1] = '\0';

    *enrich_mountpoint = xstrdup(clean_path);

    mountpoint.type = RBH_VT_STRING;
    mountpoint.string = *enrich_mountpoint;

    pair.key = "mountpoint";
    pair.value = &mountpoint;

    mountpoint_map.pairs = &pair;
    mountpoint_map.count = 1;

    if (sink_insert_info(sink, &mountpoint_map)) {
        fprintf(stderr, "Failed to set the mountpoint\n");
        return -1;
    }

    return 0;
}
