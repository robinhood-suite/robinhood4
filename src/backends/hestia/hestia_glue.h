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

#ifdef __cplusplus
}
#endif

#endif
