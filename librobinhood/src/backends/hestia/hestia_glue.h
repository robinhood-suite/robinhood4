/**
 * This file is part of RobinHood 4
 *
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */
#ifndef RBH_HESTIA_GLUE_H
#define RBH_HESTIA_GLUE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hestia_id {
    uint64_t higher;
    uint64_t lower;
};

int list_tiers(uint8_t **tiers, size_t *len);
int list_objects(uint8_t *tiers, size_t tiers_len,
                 struct hestia_id **ids, size_t *ids_len);
int list_object_attrs(struct hestia_id *id, char **obj_attrs, size_t *len);

#ifdef __cplusplus
}
#endif

#endif
