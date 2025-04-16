/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_POSIX_INTERNALS_H
#define ROBINHOOD_POSIX_INTERNALS_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <robinhood/id.h>
#include <robinhood/backends/posix_extension.h>
#include <robinhood/backend.h>

struct rbh_mut_iterator *
fts_iter_new(const char *root, const char *entry, int statx_sync_type);

int
fts_iter_root_setup(struct posix_iterator *_iter);

bool
rbh_posix_iter_is_fts(struct posix_iterator *iter);

int
load_posix_extensions(const struct rbh_backend_plugin *self,
                      struct posix_backend *posix,
                      const char *type,
                      struct rbh_config *config);

#endif
