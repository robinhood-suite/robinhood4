/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood.h>

#include "log.h"

void
print_report_log(const struct rbh_value_map *log, bool print_oneline)
{
    print_log_wrapper(log, print_oneline, NULL, NULL);
}
