/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "robinhood/backend.h"
#include "robinhood/id.h"

#include "lu_fid.h"

int
rbh_id_copy(struct rbh_id *dest, const struct rbh_id *src, char **buffer,
            size_t *bufsize)
{
    size_t size = *bufsize;
    char *data = *buffer;

    /* id->data */
    if (size < src->size) {
        errno = ENOBUFS;
        return -1;
    }

    dest->data = data;
    if (src->size > 0)
        data = mempcpy(data, src->data, src->size);
    size -= src->size;

    /* id->size */
    dest->size = src->size;

    *buffer = data;
    *bufsize = size;
    return 0;
}

bool
rbh_id_equal(const struct rbh_id *first, const struct rbh_id *second)
{
    if (first->size != second->size)
        return false;

    return !memcmp(first->data, second->data, first->size);
}

static struct rbh_id *
rbh_id_clone(const struct rbh_id *id)
{
    struct rbh_id *clone;
    size_t size;
    char *data;
    int rc;

    size = id->size;
    clone = malloc(sizeof(*clone) + size);
    if (clone == NULL)
        return NULL;
    data = (char *)clone + sizeof(*clone);

    rc = rbh_id_copy(clone, id, &data, &size);
    assert(rc == 0);

    return clone;
}


struct rbh_id *
rbh_id_new_with_id(const char *data, size_t size, unsigned short backend_id)
{
    char DATA[sizeof(data) + sizeof(short)];
    char *tmp;
    const struct rbh_id ID = {
        .data = DATA,
        .size = size + sizeof(short),
    };
    tmp = mempcpy(DATA, &backend_id, sizeof(short));
    memcpy(tmp, data, sizeof(&data));
    return rbh_id_clone(&ID);
}


struct rbh_id *
rbh_id_new(const char *data, size_t size)
{
    const struct rbh_id ID = {
        .data = data,
        .size = size,
    };

    return rbh_id_clone(&ID);
}

/* A struct file_handle has 3 public fields:
 *   - handle_bytes
 *   - handle_type
 *   - f_handle
 *
 * The data of a file handle is mapped in a struct rbh_id as follows:
 *
 * -----------------------------       ----------------------------------
 * |     file handle           |       |                ID              |
 * |---------------------------|       |--------------------------------|
 * | handle_bytes |          4 | ---=> | data  | 0x01000123456789abcdef |
 * | handle_type  | 0x01234567 |   |   | size  |                      8 | <--
 * | f_handle     | 0x89abcdef |   |   ----------------------------------   |
 * -----------------------------   |                                        |
 *                                 |                                        |
 *   + backend_id | RBH_BI_POSIX ---       sizeof(handle_type) + handle_bytes
 *                                         + sizeof(short)
 */
struct rbh_id *
rbh_id_from_file_handle(const struct file_handle *handle,
                        unsigned short backend_id)
{
    struct rbh_id *id;
    size_t size;
    char *data;

    size = sizeof(short) + sizeof(handle->handle_type)
           + handle->handle_bytes;
    id = malloc(sizeof(*id) + size);
    if (id == NULL)
        return NULL;

    data = (char *)id + sizeof(*id);

    /* id->data */
    id->data = data;
    data = mempcpy(data, &backend_id, sizeof(short));
    data = mempcpy(data, &handle->handle_type, sizeof(handle->handle_type));
    memcpy(data, handle->f_handle, handle->handle_bytes);

    /* id->size */
    id->size = size;

    return id;
}

/* Lustre file handles can be built from just a struct lu_fid.
 *
 * That is because Lustre stores a struct lustre_file_handle in the `f_handle'
 * field of the struct file_handle it produces.
 *
 * struct lustre_file_handle {
 *     struct lu_fid *child;
 *     struct lu_fid *parent;
 * };
 *
 * Where:
 *   - `child' points at the entry's lu_fid;
 *   - `parent' can probably point at the lu_fid of an entry's parent, but
 *      mostly points at an lu_fid filled with 0s.
 *
 * Therefore, the following data mapping applies:
 *
 * ---------------------------------       -------------------------------------
 * |      lu_fid                   |       |         file handle               |
 * |-------------------------------|       |-----------------------------------|
 * | sequence | 0x0123456789abcdef |  <=>  | handle_bytes |                 32 |
 * | oid      |         0Xfedcba98 |       | handle_type  |             0x0097 |
 * | version  |         0x76543210 |       | f_handle     | 0x0123456789abcdef |
 * ---------------------------------       |              |   fedcba9876543210 |
 *                                         |              |   0000000000000000 |
 *                                         |              |   0000000000000000 |
 *                                         -------------------------------------
 *
 * -------------------------------------       -----------------------------
 * |         file handle               |       |             ID            |
 * |-----------------------------------|       |----------------------------
 * | handle_bytes |                 32 |  <=>  | data | 0x0100009701234567 |
 * | handle_type  |             0x0097 |       |      |   89abcdeffdecba98 |
 * | f_handle     | 0x0123456789abcdef |       |      |   7654321000000000 |
 * |              |   fedcba9876543210 |       |      |   0000000000000000 |
 * |              |   0000000000000000 |       |      |   00000000         |
 * |              |   0000000000000000 |       | size |                 38 |
 * -------------------------------------       -----------------------------
 */
static const size_t LUSTRE_FH_SIZE = 2 * sizeof(struct lu_fid);

const size_t LUSTRE_ID_SIZE = LUSTRE_FH_SIZE + sizeof(short);

struct rbh_id *
rbh_id_from_lu_fid(const struct lu_fid *fid)
{
    struct rbh_id *id;
    unsigned short lustre_id = RBH_BI_LUSTRE;
    char *data;

    id = malloc(sizeof(*id) + LUSTRE_ID_SIZE);
    if (id == NULL)
        return NULL;
    data = (char *)id + sizeof(*id);

    /* id->data */
    id->data = data;
    data = mempcpy(data, &lustre_id, sizeof(short));
    data = mempcpy(data, fid, sizeof(*fid));
    memset(data, 0, sizeof(*fid));

    /* id->size */
    id->size = LUSTRE_ID_SIZE;

    return id;
}

const struct lu_fid *
rbh_lu_fid_from_id(const struct rbh_id *id)
{
    assert(*(int *)id->data == RBH_BI_LUSTRE);

    return (const struct lu_fid *) (id->data + sizeof(short));
}

struct file_handle *
rbh_file_handle_from_id(const struct rbh_id *id)
{
    struct file_handle *handle;

    if (id->size < sizeof(handle->handle_type)) {
        errno = EINVAL;
        return NULL;
    }

    handle = malloc(sizeof(*handle) + id->size - sizeof(short)
                    - sizeof(handle->handle_bytes));
    if (handle == NULL)
        return NULL;

    handle->handle_bytes = id->size - sizeof(short)
                           - sizeof(handle->handle_type);
    memcpy(&handle->handle_type, id->data + sizeof(short),
           sizeof(handle->handle_type));
    memcpy(&handle->f_handle, id->data + sizeof(short)
           + sizeof(handle->handle_type), handle->handle_bytes);

    return handle;
}
