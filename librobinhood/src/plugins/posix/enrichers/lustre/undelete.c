/**
 * This file is part of RobinHood 4
 *
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 * alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <error.h>
#include <libgen.h>
#include <stdio.h>

#include <lustre/lustreapi.h>

#include "robinhood/fsentry.h"
#include "robinhood/id.h"
#include "robinhood/statx.h"
#include "robinhood/sstack.h"
#include "robinhood/utils.h"

static struct rbh_sstack *fsentry_names;

static void __attribute__((destructor))
destroy_fsentry_names(void)
{
    if (fsentry_names)
        rbh_sstack_destroy(fsentry_names);
}

static int
lhsm_rebind(const struct lu_fid *old_id, const struct lu_fid *new_id,
            uint32_t hsm_archive_id, const char *dest)
{
    char old_fid_str[FID_LEN];
    char new_fid_str[FID_LEN];
    char cmd_line[512];
    int rc;

    rc = snprintf(old_fid_str, sizeof(old_fid_str), DFID, PFID(old_id));
    if (rc < 0 || rc >= sizeof(old_fid_str))
        goto out_snprintf_error;

    rc = snprintf(new_fid_str, sizeof(new_fid_str), DFID, PFID(new_id));
    if (rc < 0 || rc >= sizeof(new_fid_str))
        goto out_snprintf_error;

    rc = snprintf(cmd_line, sizeof(cmd_line),
                  "lhsmtool_posix --archive=%u -p /mnt/hsm --rebind %s %s %s",
                  hsm_archive_id, old_fid_str, new_fid_str, dest);
    if (rc < 0 || rc >= sizeof(cmd_line))
        goto out_snprintf_error;

    rc = command_call(cmd_line, NULL, NULL);
    if (rc != 0)
        fprintf(stderr, "Failed to execute rebind command\n");

    return rc;

out_snprintf_error:
    fprintf(stderr, "Error while formatting string\n");
    return -EINVAL;
}

static struct rbh_fsentry *
build_fsentry_after_undelete(const char *path, const struct lu_fid *new_fid,
                             struct rbh_fsentry *fsentry)
{
    char parent_path[PATH_MAX];
    struct rbh_id *parent_id;
    struct lu_fid parent_fid;
    char namebuf[PATH_MAX];
    struct rbh_id *new_id;
    char *name;
    char *dir;
    int rc;

    strncpy(parent_path, path, PATH_MAX);
    parent_path[PATH_MAX - 1] = '\0';

    dir = dirname(parent_path);
    rc = llapi_path2fid(dir, &parent_fid);
    if (rc) {
        fprintf(stderr, "Error while using llapi_path2fid\n");
        return NULL;
    }

    parent_id = rbh_id_from_lu_fid(&parent_fid);
    if (parent_id == NULL)
        return NULL;

    new_id = rbh_id_from_lu_fid(new_fid);
    if (new_id == NULL)
        return NULL;

    strncpy(namebuf, path, PATH_MAX);
    namebuf[PATH_MAX - 1] = '\0';
    name = basename(namebuf);

    if (fsentry_names == NULL) {
        /* This stack should only contain the names of fsentries, no need to
         * make its size larger than a path can be...
         */
        fsentry_names = rbh_sstack_new(PATH_MAX);
        if (fsentry_names == NULL)
            error(EXIT_FAILURE, errno, "Failed to allocate 'fsentry_names'");
    }

    fsentry->id = *new_id;
    fsentry->parent_id = *parent_id;
    fsentry->name = RBH_SSTACK_PUSH(fsentry_names, name, strlen(name) + 1);
    fsentry->mask |= RBH_FP_PARENT_ID | RBH_FP_NAME;

    return fsentry;
}

struct rbh_fsentry *
rbh_lustre_undelete(void *backend, const char *path,
                    struct rbh_fsentry *fsentry)
{
    const struct rbh_value *archive_id_value;
    struct rbh_fsentry *new_fsentry;
    const struct lu_fid *old_fid;
    struct lu_fid new_id = {0};
    uint32_t archive_id = 0;
    struct stat st;
    int rc = 0;

    stat_from_statx(fsentry->statx, &st);

    archive_id_value = rbh_fsentry_find_inode_xattr(fsentry, "hsm_archive_id");
    if (archive_id_value == 0) {
        fprintf(stderr, "Unable to retrieve hsm_archive_id\n");
        return NULL;
    }

    archive_id = (uint32_t) archive_id_value->int32;
    old_fid = rbh_lu_fid_from_id(&fsentry->id);

    rc = llapi_hsm_import(path, archive_id, &st, 0, -1, 0, 0, NULL, &new_id);
    if (rc) {
        fprintf(stderr, "llapi_hsm_import failed to import: %d\n", rc);
        return NULL;
    }

    rc = lhsm_rebind(old_fid, &new_id, archive_id, path);
    if (rc) {
        fprintf(stderr, "failed to rebind new_file to old_content\n");
        return NULL;
    }

    printf("'%s' has been undeleted, new FID is '"DFID"'\n", path,
           PFID(&new_id));

    new_fsentry = build_fsentry_after_undelete(path, &new_id, fsentry);
    if (new_fsentry == NULL)
        fprintf(stderr, "Failed to create new fsentry after undelete: %s (%d)",
                strerror(errno), errno);

    return new_fsentry;
}
