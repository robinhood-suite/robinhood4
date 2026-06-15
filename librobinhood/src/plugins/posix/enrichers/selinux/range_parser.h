/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_POSIX_SELINUX_PARSE_RANGE_H
#define RBH_POSIX_SELINUX_PARSE_RANGE_H

#include <stddef.h>
#include <stdint.h>

#define SELINUX_MAX_CATEGORY     1023
#define SELINUX_MAX_INTERVALS    (SELINUX_MAX_CATEGORY + 1)
#define SELINUX_MAX_SENSITIVITY  15

/**
 * An interval of SELinux categories.
 *
 * A single category such as c4 is stored as [4, 4],
 * while c7.c9 is stored as [7, 9].
 */
struct selinux_interval {
    int32_t first;
    int32_t last;
};

/*
 * A SELinux level is composed of one sensitivity and a list of
 * category intervals
 *
 * Intervals are kept ordered and adjacent intervals are merged.
 * For instance, c1,c2,c4.c6 is stored as [1, 2], [4, 6].
 */
struct selinux_level{
    int32_t sens;
    struct selinux_interval intervals[SELINUX_MAX_INTERVALS];
    size_t intervals_count;
};

/*
 * A SELinux MLS/MCS range composed of a low and a high level.
 *
 * A simple level without a '-' separator is stored with identical
 * low and high levels.
 */
struct selinux_range{
    struct selinux_level low;
    struct selinux_level high;
};

int
selinux_parse_level(char *string, struct selinux_level *level);

int
selinux_parse_range(char *range, struct selinux_range *parsed);

#endif
