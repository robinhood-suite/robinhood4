/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_FIND_UTILS_H
#define RBH_FIND_UTILS_H

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

/**
 * str2uint64_t - convert a string into a uint64_t
 *
 * @param input     a string representing an unsigned long
 * @param result    a uint64_t represented by \p string
 *
 * @return          0 on success, -1 on error
 */
int
str2uint64_t(const char *input, uint64_t *result);

#endif
