#include <vector>

#include <stdlib.h>

#include "hestia.h"
#include "hestia_glue.h"

int list_tiers(uint8_t **tiers, size_t *len)
{
    std::vector<uint8_t> hestia_tiers = hestia::list_tiers();

    *len = hestia_tiers.size();
    *tiers = (uint8_t *) calloc(1, sizeof(**tiers) * (*len));
    if (*tiers == NULL)
        return -1;

    std::copy(hestia_tiers.begin(), hestia_tiers.end(), *tiers);
    return 0;
}

int list_objects(uint8_t *tiers, size_t tiers_len,
                 struct hestia_id **ids, size_t *ids_len)
{
    *ids_len = 0;

    for (size_t i = 0; i < tiers_len; ++i) {
        std::vector<hestia::hsm_uint> hestia_objects = hestia::list(tiers[i]);
        hestia_id *ids_iter;
        void *tmp;

        if (hestia_objects.size() == 0)
            continue;

        if (*ids == NULL)
            tmp = malloc(sizeof(**ids) * hestia_objects.size());
        else
            tmp = realloc(*ids,
                          sizeof(**ids) * (*ids_len + hestia_objects.size()));

        if (tmp == NULL)
            return -1;

        *ids = static_cast<hestia_id *>(tmp);

        ids_iter = *ids + *ids_len;
        for (auto hestia_elt: hestia_objects) {
            ids_iter->higher = hestia_elt.higher;
            ids_iter->lower = hestia_elt.lower;
            ids_iter++;
        }
        *ids_len += hestia_objects.size();
    }

    return 0;
}
