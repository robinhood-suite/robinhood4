/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <robinhood.h>

int
remove_children_path(struct rbh_backend *backend, struct rbh_fsentry *entry,
                     struct rbh_list_node *batches);

struct rbh_fsevent *
get_entry_path(struct rbh_backend *backend, struct rbh_fsentry *entry);

int
chunkify_update(struct rbh_iterator *iter, struct rbh_backend *backend);
