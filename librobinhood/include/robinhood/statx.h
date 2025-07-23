/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef ROBINHOOD_STATX_H
#define ROBINHOOD_STATX_H

/** @file
 * Definition of robinhood-specific extensions for statx
 */

#include <stdint.h>
#include <sys/stat.h>

#define RBH_STATX_TYPE          0x00000001U
#define RBH_STATX_MODE          0x00000002U
#define RBH_STATX_NLINK         0x00000004U
#define RBH_STATX_UID           0x00000008U
#define RBH_STATX_GID           0x00000010U
#define RBH_STATX_ATIME_SEC     0x00000020U
#define RBH_STATX_MTIME_SEC     0x00000040U
#define RBH_STATX_CTIME_SEC     0x00000080U
#define RBH_STATX_INO           0x00000100U
#define RBH_STATX_SIZE          0x00000200U
#define RBH_STATX_BLOCKS        0x00000400U
#define RBH_STATX_BTIME_SEC     0x00000800U
#define RBH_STATX_MNT_ID        0x00001000U
#define RBH_STATX_BLKSIZE       0x40000000U
#define RBH_STATX_ATTRIBUTES    0x20000000U
#define RBH_STATX_ATIME_NSEC    0x10000000U
#define RBH_STATX_BTIME_NSEC    0x08000000U
#define RBH_STATX_CTIME_NSEC    0x04000000U
#define RBH_STATX_MTIME_NSEC    0x02000000U
#define RBH_STATX_RDEV_MAJOR    0x01000000U
#define RBH_STATX_RDEV_MINOR    0x00800000U
#define RBH_STATX_DEV_MAJOR     0x00400000U
#define RBH_STATX_DEV_MINOR     0x00200000U

#define RBH_STATX_ATIME         0x10000020U
#define RBH_STATX_BTIME         0x08000800U
#define RBH_STATX_CTIME         0x04000080U
#define RBH_STATX_MTIME         0x02000040U
#define RBH_STATX_RDEV          0x01800000U
#define RBH_STATX_DEV           0x00600000U

#define RBH_STATX_BASIC_STATS   0x57e007ffU
#define RBH_STATX_ALL           0x7fe01fffU
#define RBH_STATX_MPIFILE       0x160002fbU

struct rbh_statx_timestamp {
    int64_t tv_sec;
    uint32_t tv_nsec;
    uint32_t __reserved;
};

struct rbh_statx {
    uint32_t stx_mask;
    uint32_t stx_blksize;
    uint64_t stx_attributes;
    uint32_t stx_nlink;
    uint32_t stx_uid;
    uint32_t stx_gid;
    uint16_t stx_mode;
    uint16_t __spare0[1];
    uint64_t stx_ino;
    uint64_t stx_size;
    uint64_t stx_blocks;
    uint64_t stx_attributes_mask;
    struct rbh_statx_timestamp stx_atime;
    struct rbh_statx_timestamp stx_btime;
    struct rbh_statx_timestamp stx_ctime;
    struct rbh_statx_timestamp stx_mtime;
    uint32_t stx_rdev_major;
    uint32_t stx_rdev_minor;
    uint32_t stx_dev_major;
    uint32_t stx_dev_minor;
    uint64_t stx_mnt_id;
    uint64_t __spare2;
    uint64_t __spare3[12];
};

#define RBH_STATX_ATTR_COMPRESSED  0x00000004
#define RBH_STATX_ATTR_IMMUTABLE   0x00000010
#define RBH_STATX_ATTR_APPEND      0x00000020
#define RBH_STATX_ATTR_NODUMP      0x00000040
#define RBH_STATX_ATTR_ENCRYPTED   0x00000800
#define RBH_STATX_ATTR_AUTOMOUNT   0x00001000
#define RBH_STATX_ATTR_MOUNT_ROOT  0x00002000
#define RBH_STATX_ATTR_VERITY      0x00100000
#define RBH_STATX_ATTR_DAX         0x00200000

#define AT_RBH_STATX_SYNC_AS_STAT   0x0000
#define AT_RBH_STATX_FORCE_SYNC     0x2000
#define AT_RBH_STATX_DONT_SYNC      0x4000
#define AT_RBH_STATX_SYNC_TYPE      0x6000

void
stat_from_statx(const struct rbh_statx *statxbuf, struct stat *st);

int
rbh_statx(int dirfd, const char *restrict pathname, int flags,
          unsigned int mask, struct rbh_statx *restrict statxbuf);

void
merge_statx(struct rbh_statx *original, const struct rbh_statx *override);

uint32_t
str2statx(const char *string);

#endif
