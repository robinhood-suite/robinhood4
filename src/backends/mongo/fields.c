/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/stat.h>

#ifndef HAVE_STATX
# include "robinhood/statx.h"
#endif

#include "mongo.h"

const char *field2str(const struct rbh_filter_field *field, char **buffer,
                      size_t bufsize)
{
    switch (field->fsentry) {
    case RBH_FP_ID:
        return MFF_ID;
    case RBH_FP_PARENT_ID:
        return MFF_NAMESPACE "." MFF_PARENT_ID;
    case RBH_FP_NAME:
        return MFF_NAMESPACE "." MFF_NAME;
    case RBH_FP_SYMLINK:
        return MFF_SYMLINK;
    case RBH_FP_STATX:
        switch (field->statx) {
        case STATX_TYPE:
            return MFF_STATX "." MFF_STATX_TYPE;
        case STATX_MODE:
            return MFF_STATX "." MFF_STATX_MODE;
        case STATX_NLINK:
            return MFF_STATX "." MFF_STATX_NLINK;
        case STATX_UID:
            return MFF_STATX "." MFF_STATX_UID;
        case STATX_GID:
            return MFF_STATX "." MFF_STATX_GID;
        case STATX_ATIME:
            return MFF_STATX "." MFF_STATX_ATIME;
        case STATX_MTIME:
            return MFF_STATX "." MFF_STATX_MTIME;
        case STATX_CTIME:
            return MFF_STATX "." MFF_STATX_CTIME;
        case STATX_INO:
            return MFF_STATX "." MFF_STATX_INO;
        case STATX_SIZE:
            return MFF_STATX "." MFF_STATX_SIZE;
        case STATX_BLOCKS:
            return MFF_STATX "." MFF_STATX_BLOCKS;
        case STATX_BTIME:
            return MFF_STATX "." MFF_STATX_BTIME;
        }
        break;
    case RBH_FP_NAMESPACE_XATTRS:
        if (field->xattr == NULL)
            return MFF_NAMESPACE "." MFF_XATTRS;

        if (snprintf(*buffer, bufsize, "%s.%s", MFF_NAMESPACE "." MFF_XATTRS,
                     field->xattr) < bufsize)
            return *buffer;

        /* `*buffer' is too small */
        if (asprintf(buffer, "%s.%s", MFF_NAMESPACE "." MFF_XATTRS,
                     field->xattr) < 0)
            return NULL;

        return *buffer;
    case RBH_FP_INODE_XATTRS:
        if (field->xattr == NULL)
            return MFF_XATTRS;

        if (snprintf(*buffer, bufsize, "%s.%s", MFF_XATTRS, field->xattr)
                < bufsize)
            return *buffer;

        /* `*buffer' is too small */
        if (asprintf(buffer, "%s.%s", MFF_XATTRS, field->xattr) < 0)
            return NULL;
        return *buffer;
    }

    errno = ENOTSUP;
    return NULL;
}
