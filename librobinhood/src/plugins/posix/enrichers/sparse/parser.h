/* This file is part of RobinHood
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_EXTENSION_SPARSE_PARSER_H
#define RBH_EXTENSION_SPARSE_PARSER_H

enum sparse_predicate {
    SPRED_SPARSE,

    SPRED_MIN = SPRED_SPARSE,
    SPRED_MAX = SPRED_SPARSE,
};

/**
 * str2sparse_predicate - convert a string to an integer corresponding to a
 * sparse_predicate
 *
 * @param string          a string representing a valid predicate
 *
 * @return                the predicate that \p string represents
 *
 * This function will exit if \p string is not a valid predicate
 */
int
str2sparse_predicate(const char *string);

/**
 * predicate2str - convert a sparse_predicate to a string
 *
 * @param predicate a predicate
 *
 * @return          the string that represents \p predicate
 */
const char *
sparse_predicate2str(int predicate);

#endif
