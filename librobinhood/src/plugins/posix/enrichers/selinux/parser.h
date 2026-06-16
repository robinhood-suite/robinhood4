/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_EXTENSION_SELINUX_PARSER_H
#define RBH_EXTENSION_SELINUX_PARSER_H

enum selinux_predicate {
    SPRED_SELINUX_CTX,
    SPRED_SELINUX_USER,
    SPRED_SELINUX_ROLE,
    SPRED_SELINUX_TYPE,
    SPRED_SELINUX_RANGE,
    SPRED_SELINUX_RANGE_DOMINATES,
    SELINUX_HIGH_SENS_FIELD,
    SELINUX_HIGH_CAT_FIELD,
    INTERVAL_LAST_FIELD,
    INTERVAL_FIRST_FIELD,

    SPRED_MIN = SPRED_SELINUX_CTX,
    SPRED_MAX = INTERVAL_FIRST_FIELD,
};


/**
 * str2selinux_predicate - convert a string to an integer corresponding to a
 * selinux_predicate
 *
 * @param string          a string representing a valid predicate
 *
 * @return                the predicate that \p string represents
 *
 * This function will exit if \p string is not a valid predicate
 */
int
str2selinux_predicate(const char *string);

/**
 * selinux_predicate2str - convert a selinux_predicate to a string
 *
 * @param predicate a predicate
 *
 * @return          the string that represents \p predicate
 */
const char *
selinux_predicate2str(int predicate);

#endif
