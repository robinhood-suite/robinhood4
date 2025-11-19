/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RBH_FSEVENTS_SOURCE_LUSTRE_UTILS_H
#define RBH_FSEVENTS_SOURCE_LUSTRE_UTILS_H

#include <lustre/lustreapi.h>
#include <linux/lustre/lustre_fid.h>

#include <robinhood/value.h>
#include <robinhood/statx.h>

#include "lustre.h"

static inline time_t
cltime2sec(uint64_t cltime)
{
    /* extract secs from time field */
    return cltime >> 30;
}

static inline unsigned int
cltime2nsec(uint64_t cltime)
{
    /* extract nanosecs: */
    return cltime & ((1 << 30) - 1);
}

struct rbh_value *
fill_xattrs_fid(void *arg);

struct rbh_value *
fill_xattrs_mdt_index(void *arg);

struct rbh_value *
fill_xattrs_nb_children(void *arg);

struct rbh_value *
fill_inode_xattrs(void *arg);

struct rbh_value *
build_symlink_enrich_map(void *arg);

int
build_enrich_xattr_fsevent(struct rbh_value_map *xattrs_map,
                           char *key, struct rbh_value *value, ...);

struct rbh_id *
build_id(const struct lu_fid *fid);

int
create_statx_uid_gid(struct changelog_rec *record,
                     struct rbh_statx **rec_statx);

struct rbh_value *
build_statx_map(void *enrich_mask);

#define RBH_PATH_MAX    PATH_MAX
#define CL_BASE_FORMAT "%s: %llu %02d%-5s %u.%09u 0x%x%s t="DFID
#define CL_BASE_ARG(_mdt, _rec_) (_mdt), (_rec_)->cr_index, (_rec_)->cr_type, \
                                 changelog_type2str((_rec_)->cr_type),        \
                                 (uint32_t)cltime2sec((_rec_)->cr_time),      \
                                 cltime2nsec((_rec_)->cr_time),               \
                                 (_rec_)->cr_flags & CLF_FLAGMASK, flag_buff, \
                                 PFID(&(_rec_)->cr_tfid)
#define CL_NAME_FORMAT "p="DFID" %.*s"
#define CL_NAME_ARG(_rec_) PFID(&(_rec_)->cr_pfid), (_rec_)->cr_namelen, \
        changelog_rec_name(_rec_)
#define CL_EXT_FORMAT   "s="DFID" sp="DFID" %.*s"

void
dump_changelog(struct lustre_changelog_iterator *records,
               struct changelog_rec *record);

#endif
