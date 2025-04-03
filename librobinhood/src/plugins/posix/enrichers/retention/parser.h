/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_EXTENSION_RETENTION_PARSER_H
#define RBH_EXTENSION_RETENTION_PARSER_H

enum retention_predicate {
    RPRED_EXPIRED,
    RPRED_EXPIRED_AT,

    RPRED_MIN = RPRED_EXPIRED,
    RPRED_MAX = RPRED_EXPIRED_AT,
};

/**
 * str2retention_predicate - convert a string to an integer corresponding to a
 * retention_predicate
 *
 * @param string          a string representing a valid predicate
 *
 * @return                the predicate that \p string represents
 *
 * This function will exit if \p string is not a valid predicate
 */
int
str2retention_predicate(const char *string);

/**
 * predicate2str - convert a retention_predicate to a string
 *
 * @param predicate a predicate
 *
 * @return          the string that represents \p predicate
 */
const char *
retention_predicate2str(int predicate);

#endif
