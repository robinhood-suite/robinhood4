/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <robinhood.h>

struct rbh_mut_iterator *
get_entries(struct rbh_backend *backend, struct rbh_filter *filter);

struct rbh_fsevent *
generate_fsevent_ns_xattrs(struct rbh_fsentry *entry, struct rbh_value *value);

struct rbh_fsevent *
generate_fsevent_update_path(struct rbh_fsentry *entry,
                             struct rbh_fsentry *parent,
                             const struct rbh_value *value_path);
