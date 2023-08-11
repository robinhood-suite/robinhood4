/* This file is part of rbh-find
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_FSEVENTS_TEST_UTILS_H
#define RBH_FSEVENTS_TEST_UTILS_H

#include <robinhood/fsevent.h>
#include <robinhood/id.h>

#include "source.h"

struct source *
empty_source(void);

struct source *
event_list_source(struct rbh_fsevent *events, size_t count);

struct rbh_id *
fake_id(void);

void
fake_create(struct rbh_fsevent *fsevent, struct rbh_id *id,
            struct rbh_id *parent);

void
fake_link(struct rbh_fsevent *fsevent, struct rbh_id *id, const char *name,
          struct rbh_id *parent);

void
fake_unlink(struct rbh_fsevent *fsevent, struct rbh_id *id, const char *name,
            struct rbh_id *parent);

void
fake_delete(struct rbh_fsevent *fsevent, struct rbh_id *id);

void
fake_upsert(struct rbh_fsevent *fsevent, struct rbh_id *id, uint32_t mask,
            struct rbh_statx *statx);

#endif
