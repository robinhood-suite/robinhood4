/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_FIND_LUSTRE_PARSER_H
#define RBH_FIND_LUSTRE_PARSER_H

#include <stdbool.h>

#include <rbh-find/parser.h>

enum lustre_predicate {
    LPRED_MIN = PRED_LAST,

    LPRED_COMP_END = LPRED_MIN,
    LPRED_COMP_START,
    LPRED_EXPIRED,
    LPRED_EXPIRED_AT,
    LPRED_FID,
    LPRED_HSM_STATE,
    LPRED_IPOOL,
    LPRED_LAYOUT_PATTERN,
    LPRED_MDT_COUNT,
    LPRED_MDT_INDEX,
    LPRED_OST_INDEX,
    LPRED_POOL,
    LPRED_STRIPE_COUNT,
    LPRED_STRIPE_SIZE,

    LPRED_MAX,
};

/**
 * str2lustre_predicate - convert a string to an integer corresponding to a
 * predicate or a lustre_predicate
 *
 * @param string          a string representing a valid predicate
 *
 * @return                the predicate that \p string represents
 *
 * This function will exit if \p string is not a valid predicate
 */
int
str2lustre_predicate(const char *string);

/**
 * predicate2str - convert a predicate or lustre_predicate to a string
 *
 * @param predicate a predicate
 *
 * @return          the string that represents \p predicate
 */
const char *
lustre_predicate2str(int predicate);

#endif
