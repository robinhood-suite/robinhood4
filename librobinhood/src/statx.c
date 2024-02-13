/* This file is part of the RobinHood Library
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/stat.h>

#include "robinhood/statx.h"

#ifndef HAVE_STATX
static int
statx_timestamp_from_timespec(struct rbh_statx_timestamp *timestamp,
                              struct timespec *timespec)
{
    if (timespec->tv_sec > INT64_MAX) {
        errno = EOVERFLOW;
        return -1;
    }

    timestamp->tv_sec = timespec->tv_sec;
    timestamp->tv_nsec = timespec->tv_nsec;
    return 0;
}

static void
statx_from_stat(struct rbh_statx *statxbuf, struct stat *stat)
{
    statxbuf->stx_mask = RBH_STATX_BASIC_STATS;
    statxbuf->stx_blksize = stat->st_blksize;
    statxbuf->stx_nlink = stat->st_nlink;
    statxbuf->stx_uid = stat->st_uid;
    statxbuf->stx_gid = stat->st_gid;
    statxbuf->stx_mode = stat->st_mode;
    statxbuf->stx_ino = stat->st_ino;
    statxbuf->stx_size = stat->st_size;
    statxbuf->stx_blocks = stat->st_blocks;

    if (statx_timestamp_from_timespec(&statxbuf->stx_atime, &stat->st_atim))
        statxbuf->stx_mask &= ~RBH_STATX_ATIME;

    if (statx_timestamp_from_timespec(&statxbuf->stx_mtime, &stat->st_mtim))
        statxbuf->stx_mask &= ~RBH_STATX_MTIME;

    if (statx_timestamp_from_timespec(&statxbuf->stx_ctime, &stat->st_ctim))
        statxbuf->stx_mask &= ~RBH_STATX_CTIME;

    statxbuf->stx_rdev_major = major(stat->st_rdev);
    statxbuf->stx_rdev_minor = minor(stat->st_rdev);
    statxbuf->stx_dev_major = major(stat->st_dev);
    statxbuf->stx_dev_minor = minor(stat->st_dev);
}
#endif

static uint32_t
statx2rbh_statx_mask(uint32_t mask)
{
    mask |= RBH_STATX_ATTRIBUTES | RBH_STATX_BLKSIZE | RBH_STATX_RDEV
          | RBH_STATX_DEV;

    if (mask & RBH_STATX_ATIME_SEC)
        mask |= RBH_STATX_ATIME_NSEC;

    if (mask & RBH_STATX_BTIME_SEC)
        mask |= RBH_STATX_BTIME_NSEC;

    if (mask & RBH_STATX_CTIME_SEC)
        mask |= RBH_STATX_CTIME_NSEC;

    if (mask & RBH_STATX_MTIME_SEC)
        mask |= RBH_STATX_MTIME_NSEC;

    return mask;
}

int
rbh_statx(int dirfd, const char *restrict pathname, int flags,
          unsigned int mask, struct rbh_statx *restrict statxbuf)
{
#ifdef HAVE_STATX
    int rc;

    rc = statx(dirfd, pathname, flags, mask, (struct statx *)statxbuf);
    statxbuf->stx_mask = statx2rbh_statx_mask(statxbuf->stx_mask);
    return rc;
#else
    struct stat stat;

    /* `flags' may contain values specific to statx, let's remove them */
    if (flags & AT_RBH_STATX_FORCE_SYNC) {
        errno = ENOTSUP;
        return -1;
    }
    flags &= ~AT_RBH_STATX_SYNC_TYPE;

    if (fstatat(dirfd, pathname, &stat, flags))
        return -1;

    statx_from_stat(statxbuf, &stat);
    return 0;
#endif
}

void
merge_statx(struct rbh_statx *original, const struct rbh_statx *override)
{
    original->stx_mask |= override->stx_mask;

    if (override->stx_mask & RBH_STATX_TYPE)
        original->stx_mode |= override->stx_mode & S_IFMT;

    if (override->stx_mask & RBH_STATX_MODE)
        original->stx_mode |= override->stx_mode & ~S_IFMT;

    if (override->stx_mask & RBH_STATX_NLINK)
        original->stx_nlink = override->stx_nlink;

    if (override->stx_mask & RBH_STATX_UID)
        original->stx_uid = override->stx_uid;

    if (override->stx_mask & RBH_STATX_GID)
        original->stx_gid = override->stx_gid;

    if (override->stx_mask & RBH_STATX_ATIME_SEC)
        original->stx_atime.tv_sec = override->stx_atime.tv_sec;

    if (override->stx_mask & RBH_STATX_CTIME_SEC)
        original->stx_ctime.tv_sec = override->stx_ctime.tv_sec;

    if (override->stx_mask & RBH_STATX_MTIME_SEC)
        original->stx_mtime.tv_sec = override->stx_mtime.tv_sec;

    if (override->stx_mask & RBH_STATX_INO)
        original->stx_ino = override->stx_ino;

    if (override->stx_mask & RBH_STATX_SIZE)
        original->stx_size = override->stx_size;

    if (override->stx_mask & RBH_STATX_BLOCKS)
        original->stx_blocks = override->stx_blocks;

    if (override->stx_mask & RBH_STATX_BTIME_SEC)
        original->stx_btime.tv_sec = override->stx_btime.tv_sec;

    if (override->stx_mask & RBH_STATX_MNT_ID)
        original->stx_mnt_id = override->stx_mnt_id;

    if (override->stx_mask & RBH_STATX_BLKSIZE)
        original->stx_blksize = override->stx_blksize;

    if (override->stx_mask & RBH_STATX_ATTRIBUTES) {
        original->stx_attributes_mask = override->stx_attributes_mask;
        original->stx_attributes = override->stx_attributes;
    }

    if (override->stx_mask & RBH_STATX_ATIME_NSEC)
        original->stx_atime.tv_nsec = override->stx_atime.tv_nsec;

    if (override->stx_mask & RBH_STATX_BTIME_NSEC)
        original->stx_btime.tv_nsec = override->stx_btime.tv_nsec;

    if (override->stx_mask & RBH_STATX_CTIME_NSEC)
        original->stx_ctime.tv_nsec = override->stx_ctime.tv_nsec;

    if (override->stx_mask & RBH_STATX_MTIME_NSEC)
        original->stx_mtime.tv_nsec = override->stx_mtime.tv_nsec;

    if (override->stx_mask & RBH_STATX_RDEV_MAJOR)
        original->stx_rdev_major = override->stx_rdev_major;

    if (override->stx_mask & RBH_STATX_RDEV_MINOR)
        original->stx_rdev_minor = override->stx_rdev_minor;

    if (override->stx_mask & RBH_STATX_DEV_MAJOR)
        original->stx_dev_major = override->stx_dev_major;

    if (override->stx_mask & RBH_STATX_DEV_MINOR)
        original->stx_dev_minor = override->stx_dev_minor;
}

