/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

/* @file
 * This file contains private utility functions and macros
 *
 * Only declare utilies that are clearly needed across multiple source files.
 */

#ifndef RBH_UTILS_H
#define RBH_UTILS_H

#include <assert.h>
#include <stddef.h>
#include <limits.h>
#include <stdint.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#endif

#ifndef alignof
#define alignof _Alignof
#endif

static inline ptrdiff_t
alignoffset(void *pointer, size_t alignment)
{
    uintptr_t address = (uintptr_t)pointer;
    uintptr_t mask;

    static_assert(SIZE_MAX <= UINTPTR_MAX, "");

    assert(__builtin_popcount(alignment) == 1);
    mask = alignment - 1;
    return ((address + mask) & ~mask) - address;
}

static inline void *
ptralign(void *pointer, size_t *size, size_t alignment)
{
    ptrdiff_t offset = alignoffset(pointer, alignment);

    *size = *size > offset ? *size - offset : 0;
    return (char *)pointer + offset;
}

static inline size_t
sizealign(size_t size, size_t alignment)
{
    size_t mask;

    assert(__builtin_popcount(alignment) == 1);
    mask = alignment - 1;
    return (size + mask) & ~mask;
}

#endif
