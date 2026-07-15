/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_FSEVENTS_INFO_H
#define RBH_FSEVENTS_INFO_H

#include "enricher.h"
#include "sink.h"

/**
 * Insert the source backends, as fetched from the enricher and from the command
 * line, as information in the sink.
 *
 * @param cmd_backend      the command backend to insert
 * @param enrich_builder   the enricher from which to fetch the source backends
 * @param sink             the sink to push the information in
 *
 * @return                 0 on success, -1 on error
 */
int
insert_backend_source(char *cmd_backend,
                      struct enrich_iter_builder *enrich_builder,
                      struct sink *sink);

/**
 * Insert the enrichment mountpoint as information in the sink.
 *
 * @param enrich_builder     the enricher from which to fetch the mountpoint
 * @param sink               the sink to push the information in
 * @param enrich_mountpoint  where to store the enrich mountpoint, necessary
 *                           for logs
 *
 * @return                 0 on success, -1 on error
 */
int
insert_mountpoint(struct enrich_iter_builder *enrich_builder,
                  struct sink *sink, const char **enrich_mountpoint);

#endif
