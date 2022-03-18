/* This file is part of rbh-find
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_FIND_UTILS_H
#define RBH_FIND_UTILS_H

/**
 * shell2pcre - translate a shell pattern into a Perl Compatible regex
 *
 * @param shell the shell pattern to convert
 *
 * @return      a pointer to the newly allocated perl compatible regex
 *
 * The conversion prefixes any '*' and '?' with a dot unless it is escaped.
 * A leading '^' and a trailing '(?!\n)$' are added to the pattern.
 * The '(?!\n)' part is meant as a workaround to PCRE's specification that
 * allows '$' to match either before or after the last trailing newline.
 *
 * The returned pointer must be freed by the caller.
 */
char *
shell2pcre(const char *shell);

enum time_unit {
    TU_SECOND,
    TU_MINUTE,
    TU_HOUR,
    TU_DAY,
};

extern const unsigned long TIME_UNIT2SECONDS[];

/**
 * str2seconds - convert a string into a number of seconds
 *
 * @param unit      a time unit in which to interpret \p string
 * @param string    a string representing an unsigned long
 *
 * @return          an unsigned long representing \p string converted into
 *                  seconds
 */
unsigned long
str2seconds(enum time_unit unit, const char *string);

#endif
