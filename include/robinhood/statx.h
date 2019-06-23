/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 * 		      alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifndef ROBINHOOD_STATX_H
#define ROBINHOOD_STATX_H

/** @file
 * Definition of statx structures for compatability with older systems
 *
 * Refer to statx(2) for information on the structures and the constants defined
 * in this file.
 */

#include <stdint.h>

struct statx_timestamp {
    int64_t tv_sec;
    uint32_t tv_nsec;
};

struct statx {
    uint32_t stx_mask;
    uint32_t stx_blksize;
    uint64_t stx_attributes;
    uint32_t stx_nlink;
    uint32_t stx_uid;
    uint32_t stx_gid;
    uint16_t stx_mode;
    uint64_t stx_ino;
    uint64_t stx_size;
    uint64_t stx_blocks;
    uint64_t stx_attributes_mask;

    struct statx_timestamp stx_atime;
    struct statx_timestamp stx_btime;
    struct statx_timestamp stx_ctime;
    struct statx_timestamp stx_mtime;

    uint32_t stx_rdev_major;
    uint32_t stx_rdev_minor;

    uint32_t stx_dev_major;
    uint32_t stx_dev_minor;
};

#define STATX_TYPE          0x000000001U
#define STATX_MODE          0x000000002U
#define STATX_NLINK         0x000000004U
#define STATX_UID           0x000000008U
#define STATX_GID           0x000000010U
#define STATX_ATIME         0x000000020U
#define STATX_MTIME         0x000000040U
#define STATX_CTIME         0x000000080U
#define STATX_INO           0x000000100U
#define STATX_SIZE          0x000000200U
#define STATX_BLOCKS        0x000000400U
#define STATX_BASIC_STATS   0x0000007ffU
#define STATX_BTIME         0x000000800U
#define STATX_ALL           0x000000fffU

#define AT_STATX_SYNC_TYPE    0x6000
#define AT_STATX_SYNC_AS_STAT 0x0000
#define AT_STATX_FORCE_SYNC   0x2000
#define AT_STATX_DONT_SYNC    0x4000

#endif
