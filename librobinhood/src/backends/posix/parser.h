/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_POSIX_PARSER_H
#define RBH_POSIX_PARSER_H

enum predicate {
    PRED_AMIN,
    PRED_ANEWER,
    PRED_ATIME,
    PRED_BLOCKS,
    PRED_BMIN,
    PRED_BNEWER,
    PRED_BTIME,
    PRED_CMIN,
    PRED_CNEWER,
    PRED_CONTEXT,
    PRED_CTIME,
    PRED_EMPTY,
    PRED_EXECUTABLE,
    PRED_FALSE,
    PRED_FSTYPE,
    PRED_GID,
    PRED_GROUP,
    PRED_ILNAME,
    PRED_INAME,
    PRED_INUM,
    PRED_IPATH,
    PRED_IREGEX,
    PRED_IWHOLENAME,
    PRED_LINKS,
    PRED_LNAME,
    PRED_MMIN,
    PRED_MTIME,
    PRED_NAME,
    PRED_NEWER,
    PRED_NEWERXY,
    PRED_NOGROUP,
    PRED_NOUSER,
    PRED_PATH,
    PRED_PERM,
    PRED_READABLE,
    PRED_REGEX,
    PRED_SAMEFILE,
    PRED_SIZE,
    PRED_TRUE,
    PRED_TYPE,
    PRED_UID,
    PRED_USED,
    PRED_USER,
    PRED_WHOLENAME,
    PRED_WRITEABLE,
    PRED_XATTR,
    PRED_XTYPE,

    PRED_LAST,
};

/**
 * str2predicate - convert a string to a predicate
 *
 * @param string    a string representing a valid predicate
 *
 * @return          the predicate that \p string represents
 *
 * This function will exit if \p string is not a valid predicate
 */
enum predicate
str2predicate(const char *string);

/**
 * predicate2str - convert a predicate to a string
 *
 * @param predicate a predicate
 *
 * @return          the string that represents \p predicate
 */
const char *
predicate2str(enum predicate);

#endif
