/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

/* @file
 * This file contains private utility functions and macros
 *
 * Only declare utilies that are clearly needed across multiple source files.
 */

#ifndef RBH_UTILS_H
#define RBH_UTILS_H

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
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

    assert(offset >= 0);
    *size = *size > (size_t)offset ? *size - offset : 0;
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

static inline void *
aligned_memalloc(size_t alignment, size_t size, char **buffer, size_t *_bufsize)
{
    size_t bufsize = *_bufsize;
    char *mem = *buffer;

    mem = ptralign(mem, &bufsize, alignment);
    if (bufsize < size) {
        errno = ENOBUFS;
        return NULL;
    }
    bufsize -= size;

    *buffer = mem + size;
    *_bufsize = bufsize;
    return mem;
}

#endif
