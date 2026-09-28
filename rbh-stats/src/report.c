/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood.h>

#include "stats.h"

void
print_report_stats(const struct rbh_value_map *stats,
                 enum output_format output_format)
{
    print_stats_wrapper(stats, output_format, NULL, NULL);
}
