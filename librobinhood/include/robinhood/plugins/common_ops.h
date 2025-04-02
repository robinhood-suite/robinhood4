/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_BACKEND_COMMON_OPS_H
#define ROBINHOOD_BACKEND_COMMON_OPS_H

/**
 * Define a list of operations common to both plugins and extensions.
 */
struct rbh_pe_common_operations {
    enum rbh_parser_token (*check_valid_token)(const char *token);
    struct rbh_filter *(*build_filter)(const char **argv, int argc, int *index,
                                       bool *need_prefetch);
    int (*fill_entry_info)(char *output, int max_length,
                           const struct rbh_fsentry *fsentry,
                           const char *directive, const char *backend);
    int (*delete_entry)(struct rbh_fsentry *fsentry);
};

#endif
