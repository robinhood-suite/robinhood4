/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_EXTENSION_ACL_PARSER_H
#define RBH_EXTENSION_ACL_PARSER_H

enum acl_predicate {
    APRED_USER,
    APRED_GROUP,
    APRED_DEFAULT_USER,
    APRED_DEFAULT_GROUP,
    ACL_ID_FIELD,

    APRED_MIN = APRED_USER,
    APRED_MAX = ACL_ID_FIELD,
};


/**
 * str2acl_predicate - convert a string to an integer corresponding to a
 * acl_predicate
 *
 * @param string          a string representing a valid predicate
 *
 * @return                the predicate that \p string represents
 *
 * This function will exit if \p string is not a valid predicate
 */
int
str2acl_predicate(const char *string);

/**
 * acl_predicate2str - convert a acl_predicate to a string
 *
 * @param predicate a predicate
 *
 * @return          the string that represents \p predicate
 */
const char *
acl_predicate2str(int predicate);

#endif
