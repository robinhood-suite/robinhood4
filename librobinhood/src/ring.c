/* This file is part of the RobinHood Library
 * Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/syscall.h>

#include "robinhood/ring.h"
#include "ring.h"

struct rbh_ring *
rbh_ring_new(size_t size)
{
    struct rbh_ring *ring;
    void *buffer;
    int fd;

    fd = syscall(SYS_memfd_create, "ring", 0);
    if (fd < 0)
        return NULL;

    if (ftruncate(fd, size)) {
        int save_errno = errno;

        close(fd);
        errno = save_errno;
        return NULL;
    }

    /* Reserve a range in the process' address space */
    buffer = mmap(NULL, size << 1, PROT_READ | PROT_WRITE,
                  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (buffer == MAP_FAILED) {
        int save_errno = errno;

        close(fd);
        errno = save_errno;
        return NULL;
    }

    if (mmap(buffer, size, PROT_READ | PROT_WRITE,
             MAP_SHARED | MAP_FIXED, fd, 0) == MAP_FAILED) {
        int save_errno = errno;

        munmap(buffer, size << 1);
        close(fd);
        errno = save_errno;
        return NULL;
    }

    if (mmap(buffer + size, size, PROT_READ | PROT_WRITE,
             MAP_SHARED | MAP_FIXED, fd, 0) == MAP_FAILED) {
        int save_errno = errno;

        munmap(buffer, size << 1);
        close(fd);
        errno = save_errno;
        return NULL;
    }

    if (close(fd)) {
        int save_errno = errno;

        munmap(buffer, size << 1);
        errno = save_errno;
        return NULL;
    }

    ring = malloc(sizeof(*ring));
    if (ring == NULL) {
        int save_errno = errno;

        munmap(buffer, size << 1);
        errno = save_errno;
        return NULL;
    }

    ring->size = size;
    ring->used = 0;
    ring->head = ring->data = buffer;

    return ring;
}

void *
rbh_ring_push(struct rbh_ring *ring, const void *data, size_t size)
{
    char *tail = ring->head + ring->used;

    if (size == 0)
        return tail;

    if (ring->size - ring->used < size) {
        errno = size > ring->size ? EINVAL : ENOBUFS;
        return NULL;
    }

    ring->used += size;
    if (data != NULL)
        memcpy(tail, data, size);

    return tail;
}

void *
rbh_ring_peek(struct rbh_ring *ring, size_t *readable)
{
    *readable = ring->used;
    return ring->head;
}

int
rbh_ring_pop(struct rbh_ring *ring, size_t count)
{
    if (count > ring->used) {
        errno = EINVAL;
        return -1;
    }

    ring->head += count;
    if (ring->head >= ring->data + ring->size)
        ring->head -= ring->size;

    ring->used -= count;
    return 0;
}

void
rbh_ring_destroy(struct rbh_ring *ring)
{
    munmap(ring->data, ring->size << 1);
    free(ring);
}
