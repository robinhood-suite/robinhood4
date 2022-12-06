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
