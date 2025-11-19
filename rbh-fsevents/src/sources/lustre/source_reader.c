/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include <robinhood/itertools.h>
#include <robinhood/statx.h>

#include <lustre/lustreapi.h>
#include <linux/lustre/lustre_fid.h>

#include "lustre.h"
#include "lustre_utils.h"

#include "utils.h"

static int
build_statx_event(uint32_t statx_enrich_mask, struct rbh_fsevent *fsevent,
                  struct rbh_statx *rec_statx)
{
    fsevent->type = RBH_FET_UPSERT;
    fsevent->upsert.statx = rec_statx;

    fsevent->xattrs = build_enrich_map(build_statx_map,
                                       &statx_enrich_mask);
    if (fsevent->xattrs.pairs == NULL)
        return -1;

    return 0;
}

static int
new_link_inode_event(struct changelog_rec *record, struct rbh_fsevent *fsevent)
{
    char *data;

    fsevent->type = RBH_FET_LINK;

    fsevent->xattrs = build_enrich_map(build_empty_map, "path");
    if (fsevent->xattrs.pairs == NULL)
        return -1;

    fsevent->link.parent_id = build_id(&record->cr_pfid);
    if (fsevent->link.parent_id == NULL)
        return -1;

    data = source_stack_alloc(NULL, record->cr_namelen + 1);
    if (data == NULL)
        return -1;
    memcpy(data, changelog_rec_name(record), record->cr_namelen);
    data[record->cr_namelen] = '\0';
    fsevent->link.name = data;

    return 0;
}

static int
update_statx_without_uid_gid_event(struct changelog_rec *record,
                                   struct rbh_fsevent *fsevent)
{
    struct rbh_statx *rec_statx;
    uint32_t statx_enrich_mask;

    if (create_statx_uid_gid(record, &rec_statx))
        return -1;

    statx_enrich_mask = RBH_STATX_ALL ^ RBH_STATX_UID ^ RBH_STATX_GID;
    if (build_statx_event(statx_enrich_mask, fsevent, rec_statx))
        return -1;

    return 0;
}

static int
update_parent_acmtime_event(struct lu_fid *parent_id,
                            struct rbh_fsevent *fsevent)
{
    uint32_t statx_enrich_mask;
    struct rbh_id *id;

    id = build_id(parent_id);
    if (id == NULL)
        return -1;

    fsevent->id.data = id->data;
    fsevent->id.size = id->size;

    /* Also, retrieve the type because we need it when enriching the entry */
    statx_enrich_mask = RBH_STATX_TYPE | RBH_STATX_ATIME | RBH_STATX_CTIME |
                        RBH_STATX_MTIME;
    if (build_statx_event(statx_enrich_mask, fsevent, NULL))
        return -1;

    return 0;
}

static int
update_parent_nb_children_event(struct lu_fid *parent_id, int64_t inc,
                                struct rbh_fsevent *fsevent)
{
    struct rbh_id *id;
    id = build_id(parent_id);
    if (id == NULL)
        return -1;

    fsevent->id.data = id->data;
    fsevent->id.size = id->size;

    fsevent->type = RBH_FET_XATTR;
    if (build_enrich_xattr_fsevent(&fsevent->xattrs,
                                   "nb_children",
                                   fill_xattrs_nb_children(&inc),
                                   NULL))
        return -1;

    return 0;
}

/* Initialize a batch of \p n_events fsevents, and copy a given rbh_id \p id
 * to each event in the batch.
 */
static struct rbh_fsevent *
fsevent_list_alloc(size_t n_events, struct rbh_id *id)
{
    struct rbh_fsevent *new_events;
    size_t i;

    new_events = source_stack_alloc(NULL, sizeof(*new_events) * n_events);
    if (new_events == NULL)
        error(EXIT_FAILURE, errno,
              "source_stack_alloc in fsevent_list_alloc for '%lu' events",
              n_events);

    memset(new_events, 0, sizeof(*new_events) * n_events);

    for (i = 0; i < n_events; ++i) {
        new_events[i].id.data = id->data;
        new_events[i].id.size = id->size;
    }

    return new_events;
}

static int
build_create_inode_events(struct changelog_rec *record, struct rbh_id *id,
                          struct rbh_iterator **fsevents_iterator)
{
    struct rbh_fsevent *new_events;

    new_events = fsevent_list_alloc(5, id);

    if (new_link_inode_event(record, &new_events[0]))
        return -1;

    new_events[1].type = RBH_FET_XATTR;
    if (build_enrich_xattr_fsevent(&new_events[1].xattrs,
                                   "fid",
                                   fill_xattrs_fid(record),
                                   "rbh-fsevents",
                                   build_empty_map("lustre"),
                                   NULL))
        return -1;

    if (update_statx_without_uid_gid_event(record, &new_events[2]))
        return -1;

    /* Update the parent information after creating a new entry */
    if (update_parent_acmtime_event(&record->cr_pfid, &new_events[3]))
        return -1;

    if (update_parent_nb_children_event(&record->cr_pfid, 1, &new_events[4]))
        return -1;

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 5,
                                        NULL);

    return 0;
}

static int
build_setxattr_event(struct changelog_rec *record, struct rbh_id *id,
                     struct rbh_iterator **fsevents_iterator)
{
    char *xattr = changelog_rec_xattr(record)->cr_xattr;
    struct rbh_fsevent *new_events;
    uint32_t statx_enrich_mask = 0;

    new_events = fsevent_list_alloc(3, id);

    statx_enrich_mask = RBH_STATX_CTIME_SEC | RBH_STATX_CTIME_NSEC;
    if (build_statx_event(statx_enrich_mask, &new_events[0], NULL))
        return -1;

    new_events[1].type = RBH_FET_XATTR;
    new_events[1].xattrs = build_enrich_map(fill_inode_xattrs, xattr);
    if (new_events[1].xattrs.pairs == NULL)
        return -1;

    new_events[2].type = RBH_FET_XATTR;
    if (build_enrich_xattr_fsevent(&new_events[2].xattrs,
                                   "rbh-fsevents",
                                   build_empty_map("lustre"),
                                   NULL))
        return -1;

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 3,
                                        NULL);

    return 0;
}

static int
build_statx_update_event(uint32_t statx_enrich_mask, struct rbh_id *id,
                         struct rbh_iterator **fsevents_iterator)
{
    struct rbh_fsevent *new_events;

    new_events = fsevent_list_alloc(2, id);

    if (build_statx_event(statx_enrich_mask, &new_events[0], NULL))
        return -1;

    new_events[1].type = RBH_FET_XATTR;
    if (build_enrich_xattr_fsevent(&new_events[1].xattrs,
                                   "rbh-fsevents",
                                   build_empty_map("lustre"),
                                   NULL))
        return -1;

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 2,
                                        NULL);

    return 0;
}

/* Do the exact same operations as for creating an inode, except for an
 * additional one that is the enrichment of the symlink target
 */
static int
build_softlink_events(struct changelog_rec *record, struct rbh_id *id,
                      int32_t mdt_index,
                      struct rbh_iterator **fsevents_iterator)
{
    struct rbh_fsevent *new_events;

    new_events = fsevent_list_alloc(6, id);

    if (new_link_inode_event(record, &new_events[0]))
        return -1;

    new_events[1].type = RBH_FET_XATTR;
    if (build_enrich_xattr_fsevent(&new_events[1].xattrs,
                                   "fid",
                                   fill_xattrs_fid(record),
                                   "mdt_index",
                                   fill_xattrs_mdt_index(&mdt_index),
                                   "rbh-fsevents",
                                   build_empty_map("lustre"),
                                   NULL))
        return -1;

    if (update_statx_without_uid_gid_event(record, &new_events[2]))
        return -1;

    /* Update the parent information after creating a new entry */
    if (update_parent_acmtime_event(&record->cr_pfid, &new_events[3]))
        return -1;

    if (update_parent_nb_children_event(&record->cr_pfid, 1, &new_events[4]))
        return -1;

    /* Mark the event for enrichment of the symlink target */
    new_events[5].type = RBH_FET_UPSERT;
    new_events[5].xattrs = build_enrich_map(build_symlink_enrich_map, NULL);
    if (new_events[5].xattrs.pairs == NULL)
        return -1;

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 6,
                                        NULL);

    return 0;
}

static int
build_hardlink_or_mknod_events(struct changelog_rec *record, struct rbh_id *id,
                               int32_t mdt_index,
                               struct rbh_iterator **fsevents_iterator)
{
    struct rbh_fsevent *new_events;
    int nb_events;
    int i = 0;

    /* For hardlinks, we must create a new ns entry for the target, update its
     * statx attributes and the statx attributes of the parent directory of the
     * link. We don't need to retrieve the xattrs of the link, since they are
     * the same of those of the target.
     *
     * For special files like named pipes, we must do the same operations as
     * hardlinks, and not retrieve xattrs aswell since they cannot have xattrs.
     *
     * Therefore, the build of a hardlink or mknod event is subset of the
     * operations done to build a inode creation event.
     */
    if (record->cr_type == CL_MKNOD)
        nb_events = 5;
    else
        nb_events = 4;

    new_events = fsevent_list_alloc(nb_events, id);

    if (new_link_inode_event(record, &new_events[i++]))
        return -1;

    if (record->cr_type == CL_MKNOD) {
        new_events[i].type = RBH_FET_XATTR;
        if (build_enrich_xattr_fsevent(&new_events[i++].xattrs, "fid",
                                       fill_xattrs_fid(record), "mdt_index",
                                       fill_xattrs_mdt_index(&mdt_index), NULL))
            return -1;
    }

    if (update_statx_without_uid_gid_event(record, &new_events[i++]))
        return -1;

    /* Update the parent information after creating a new entry */
    if (update_parent_acmtime_event(&record->cr_pfid, &new_events[i++]))
        return -1;

    if (update_parent_nb_children_event(&record->cr_pfid, 1, &new_events[i]))
        return -1;

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events),
                                        nb_events, NULL);

    return 0;
}

static int
unlink_inode_event(struct lu_fid *parent_id, char *name, size_t namelen,
                   bool last_copy, struct rbh_fsevent *fsevent,
                   time_t cr_time, bool last_copy_archived)
{
    char *data;

    fsevent->xattrs.count = 0;

    /* If the unlinked target is the last link:
     *  - If it has a copy archived, delete the name and parent_id, and store
     *  the path and rm_time.
     *  - delete the entry altogether.
     */
    if (last_copy) {
        if (last_copy_archived) {
            fsevent->type = RBH_FET_PARTIAL_UNLINK;
            fsevent->rm_time = cltime2sec(cr_time);

            return 0;
        }

        fsevent->type = RBH_FET_DELETE;
        return 0;
    }

    fsevent->type = RBH_FET_UNLINK;

    fsevent->link.parent_id = build_id(parent_id);
    if (fsevent->link.parent_id == NULL)
        return -1;

    data = source_stack_alloc(NULL, namelen + 1);
    if (data == NULL)
        return -1;
    memcpy(data, name, namelen);
    data[namelen] = '\0';
    fsevent->link.name = data;

    return 0;
}

static int
build_unlink_or_rmdir_events(struct changelog_rec *record, struct rbh_id *id,
                             struct rbh_iterator **fsevents_iterator)
{
    bool last_copy = record->cr_flags & CLF_UNLINK_LAST;
    bool last_copy_archived = record->cr_flags & CLF_UNLINK_HSM_EXISTS;
    struct rbh_fsevent *new_events;

    new_events = fsevent_list_alloc(3, id);

    if (unlink_inode_event(&record->cr_pfid, changelog_rec_name(record),
                           record->cr_namelen, last_copy, &new_events[0],
                           record->cr_time, last_copy_archived))
        return -1;

    if (update_parent_acmtime_event(&record->cr_pfid, &new_events[1]))
        return -1;

    if (update_parent_nb_children_event(&record->cr_pfid, -1, &new_events[2]))
        return -1;

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 3,
                                        NULL);

    return 0;
}

/* Renames are a combination of 6 values :
 * source fid, source parent fid, source name
 * target fid, target parent fid, target name
 *
 * Since we have no way with fsevents to modify a current link
 * parent/name/path, we instead unlink the current link using the source
 * values, and create a new link for the target, but they all share the same
 * inodes.
 *
 * If the rename overwrote data, we must also unlink that link from the DB. This
 * information is recorded in the target fid of the record, so if it is not 0,
 * that means data was overwriten.
 */
static int
build_rename_events(struct changelog_rec *record, struct rbh_id *id,
                    struct rbh_iterator **fsevents_iterator)
{
    struct changelog_ext_rename *rename_log = changelog_rec_rename(record);
    /* If the overwriten link is the last one and it has no HSM copy */
    bool last_copy = record->cr_flags & CLF_RENAME_LAST;
    bool last_copy_archived = record->cr_flags & CLF_RENAME_LAST_EXISTS;
    struct rbh_fsevent *new_events;
    struct rbh_id *renamed_id;
    int n_events = 7;
    int counter = 0;

    renamed_id = build_id(&rename_log->cr_sfid);
    if (renamed_id == NULL)
        return -1;

    /* Create the batch of events and give them the ID of the renamed entry */
    new_events = fsevent_list_alloc(n_events, renamed_id);

    /* If an entry is overwritten, simply change the target ID of the unlink to
     * that one.
     */
    if (!fid_is_zero(&record->cr_tfid)) {
        new_events[counter].id.data = id->data;
        new_events[counter].id.size = id->size;

        if (unlink_inode_event(&record->cr_pfid, changelog_rec_name(record),
                               record->cr_namelen, last_copy,
                               &new_events[counter], record->cr_time,
                               last_copy_archived))
             return -1;

        counter++;
    }

    if (new_link_inode_event(record, &new_events[counter]))
        return -1;

    counter++;

    if (update_statx_without_uid_gid_event(record, &new_events[counter]))
        return -1;

    counter++;

    /* Update the parent information after creating a new entry */
    if (update_parent_acmtime_event(&record->cr_pfid, &new_events[counter]))
        return -1;

    counter++;

    if (fid_is_zero(&record->cr_tfid)) {
        if (update_parent_nb_children_event(&record->cr_pfid, 1,
                                            &new_events[counter]))
            return -1;

        counter++;
    }

    if (unlink_inode_event(&rename_log->cr_spfid, changelog_rec_sname(record),
                           changelog_rec_snamelen(record), false,
                           &new_events[counter], record->cr_time, false))
        return -1;

    counter++;

    if (update_parent_acmtime_event(&rename_log->cr_spfid,
                                    &new_events[counter]))
        return -1;

    counter++;

    if (update_parent_nb_children_event(&rename_log->cr_spfid, -1,
                                        &new_events[counter]))
        return -1;


    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events),
                                        n_events, NULL);

    return 0;
}

/* In the future we will need to modify this function to create two events for
 * releases and restores, because they both modify the layout of the file as
 * well as the hsm state. Currently, these operations both trigger a CL_LAYOUT
 * changelog, so the layout modification will be managed in that event's
 * associated function.
 */
static int
build_hsm_events(struct rbh_id *id, struct rbh_iterator **fsevents_iterator)
{
    uint32_t statx_enrich_mask = RBH_STATX_BLOCKS;
    struct rbh_fsevent *new_events;

    new_events = fsevent_list_alloc(4, id);

    if (build_statx_event(statx_enrich_mask, &new_events[0], NULL))
        return -1;

    new_events[1].type = RBH_FET_XATTR;

    /* Mark this fsevent for Lustre enrichment to retrieve all Lustre
     * values. Will be changed later to retrieve only the modified values,
     * i.e. archive id, hsm state and layout.
     */
    if (build_enrich_xattr_fsevent(&new_events[1].xattrs,
                                   "rbh-fsevents", build_empty_map("lustre"),
                                   NULL))
        return -1;

    new_events[2].type = RBH_FET_XATTR;
    new_events[2].xattrs = build_enrich_map(fill_inode_xattrs, "trusted.lov");
    if (new_events[2].xattrs.pairs == NULL)
        return -1;

    new_events[3].type = RBH_FET_XATTR;
    new_events[3].xattrs = build_enrich_map(fill_inode_xattrs, "trusted.hsm");
    if (new_events[3].xattrs.pairs == NULL)
        return -1;

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 4,
                                        NULL);

    return 0;
}

static int
build_layout_events(struct rbh_id *id, struct rbh_iterator **fsevents_iterator)
{
    uint32_t statx_enrich_mask = RBH_STATX_CTIME_SEC | RBH_STATX_CTIME_NSEC;
    struct rbh_fsevent *new_events;

    new_events = fsevent_list_alloc(2, id);

    if (build_statx_event(statx_enrich_mask, &new_events[0], NULL))
        return -1;

    new_events[1].type = RBH_FET_XATTR;

    /* Mark this fsevent for Lustre enrichment to retrieve all Lustre
     * values. Will be changed later to retrieve only the modified values,
     * i.e. archive id, hsm state and layout.
     */
    if (build_enrich_xattr_fsevent(&new_events[1].xattrs,
                                   "rbh-fsevents", build_empty_map("lustre"),
                                   NULL))
        return -1;

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 2,
                                        NULL);

    return 0;
}

/* FLRW events are events that happen when writing data to a mirrored file in
 * Lustre. When doing so, it will write data to the "main" copy, and set a
 * "dirty" flag to the other copies signifying a synchronization is necessary.
 * As such, managing this event is the same as managing a truncate + retrieving
 * the Lustre information for that file.
 */
static int
build_flrw_events(struct rbh_id *id, struct rbh_iterator **fsevents_iterator)
{
    uint32_t statx_enrich_mask = RBH_STATX_CTIME_SEC | RBH_STATX_CTIME_NSEC |
                                 RBH_STATX_MTIME_SEC | RBH_STATX_MTIME_NSEC |
                                 RBH_STATX_BLOCKS | RBH_STATX_SIZE;
    struct rbh_fsevent *new_events;

    new_events = fsevent_list_alloc(2, id);

    if (build_statx_event(statx_enrich_mask, &new_events[0], NULL))
        return -1;

    new_events[1].type = RBH_FET_XATTR;
    if (build_enrich_xattr_fsevent(&new_events[1].xattrs,
                                   "rbh-fsevents", build_empty_map("lustre"),
                                   NULL))
        return -1;

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 2,
                                        NULL);

    return 0;
}

/* Resync events are events that happen when synchronizing a mirrored file in
 * Lustre. When doing so, all mirrors of a file will copy the data from the
 * "main" mirror, and remove the "dirty" flags from the other copies. As such,
 * managing this event is the same as managing a FLRW event, minus a few statx
 * values.
 */
static int
build_resync_events(struct rbh_id *id, struct rbh_iterator **fsevents_iterator)
{
    uint32_t statx_enrich_mask = RBH_STATX_CTIME_SEC | RBH_STATX_CTIME_NSEC |
                                 RBH_STATX_BLOCKS;
    struct rbh_fsevent *new_events;

    new_events = fsevent_list_alloc(2, id);

    if (build_statx_event(statx_enrich_mask, &new_events[0], NULL))
        return -1;

    new_events[1].type = RBH_FET_XATTR;
    if (build_enrich_xattr_fsevent(&new_events[1].xattrs,
                                   "rbh-fsevents", build_empty_map("lustre"),
                                   NULL))
        return -1;

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 2,
                                        NULL);

    return 0;
}

/* Migrate events only correspond to metadata changes, meaning we only have to
 * change the target and target's parent striping information.
 */
static int
build_migrate_events(struct changelog_rec *record, struct rbh_id *id,
                     struct rbh_iterator **fsevents_iterator)
{
    struct changelog_ext_rename *migrate_log = changelog_rec_rename(record);
    struct rbh_fsevent *new_events;
    struct rbh_id *migrated_id;

    migrated_id = build_id(&migrate_log->cr_sfid);
    if (migrated_id == NULL)
        return -1;

    new_events = fsevent_list_alloc(6, id);

    /* This new link is necessary because a metadata migration changes the
     * FID of the entry.
     */
    if (new_link_inode_event(record, &new_events[0]))
        return -1;

    if (update_statx_without_uid_gid_event(record, &new_events[1]))
        return -1;

    /* Update the parent information after creating a new entry */
    if (update_parent_acmtime_event(&record->cr_pfid, &new_events[2]))
        return -1;

    new_events[3].id.data = migrated_id->data;
    new_events[3].id.size = migrated_id->size;
    if (unlink_inode_event(&migrate_log->cr_spfid, changelog_rec_name(record),
                           changelog_rec_snamelen(record), true,
                           &new_events[3], 0, false))
        return -1;

    if (update_parent_acmtime_event(&migrate_log->cr_spfid, &new_events[4]))
        return -1;

    new_events[5].type = RBH_FET_XATTR;
    if (build_enrich_xattr_fsevent(&new_events[5].xattrs,
                                   "rbh-fsevents", build_empty_map("lustre"),
                                   NULL))
        return -1;

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 6,
                                        NULL);

    return 0;
}

const void *
lustre_changelog_iter_next(void *iterator)
{
    struct lustre_changelog_iterator *records = iterator;
    struct rbh_iterator *fsevents_iterator;
    const struct rbh_fsevent *to_return;
    uint32_t statx_enrich_mask = 0;
    struct changelog_rec *record;
    struct rbh_id *id;
    int save_errno;
    int rc;

    flush_source_stack();

    fsevents_iterator = records->fsevents_iterator;

    if (fsevents_iterator) {
        to_return = rbh_iter_next(fsevents_iterator);
        if (to_return != NULL)
            return to_return;

        rbh_iter_destroy(fsevents_iterator);
        records->fsevents_iterator = NULL;
        errno = 0;
    }

    if (records->max_changelog > 0 &&
        records->max_changelog == records->nb_changelog) {
        records->empty = true;
        errno = ENODATA;
        return NULL;
    }

retry:
    rc = llapi_changelog_recv(records->reader, &record);
    if (rc > 0 || rc == -EAGAIN) {
        records->empty = true;
        errno = ENODATA;
        return NULL;
    } else if (rc < 0) {
        errno = -rc;
        return NULL;
    }

    records->last_changelog_index = record->cr_index;
    records->nb_changelog++;

    if (records->dump_file)
        dump_changelog(records, record);

    id = build_id(&record->cr_tfid);
    if (id == NULL) {
        rc = -1;
        goto end_event;
    }

    switch (record->cr_type) {
    case CL_CREATE:
    case CL_MKDIR:
        rc = build_create_inode_events(record, id, &records->fsevents_iterator);
        break;
    case CL_SETXATTR:
        rc = build_setxattr_event(record, id, &records->fsevents_iterator);
        break;
    case CL_SETATTR:
        statx_enrich_mask = RBH_STATX_ALL;
        /* fall through */
    case CL_CLOSE:
    case CL_MTIME:
        statx_enrich_mask |= RBH_STATX_MTIME_SEC | RBH_STATX_MTIME_NSEC |
                             RBH_STATX_SIZE | RBH_STATX_BLOCKS;
        /* fall through */
    case CL_CTIME:
        statx_enrich_mask |= RBH_STATX_CTIME_SEC | RBH_STATX_CTIME_NSEC;
        /* fall through */
    case CL_ATIME:
        statx_enrich_mask |= RBH_STATX_ATIME_SEC | RBH_STATX_ATIME_NSEC;
        rc = build_statx_update_event(statx_enrich_mask, id,
                                      &records->fsevents_iterator);
        break;
    case CL_SOFTLINK:
        rc = build_softlink_events(record, id, records->source_mdt_index,
                                   &records->fsevents_iterator);
        break;
    case CL_HARDLINK:
    case CL_MKNOD:
        rc = build_hardlink_or_mknod_events(record, id,
                                            records->source_mdt_index,
                                            &records->fsevents_iterator);
        break;
    case CL_RMDIR:
    case CL_UNLINK:
        rc = build_unlink_or_rmdir_events(record, id,
                                          &records->fsevents_iterator);
        break;
    case CL_RENAME:
        rc = build_rename_events(record, id, &records->fsevents_iterator);
        break;
    case CL_HSM:
        rc = build_hsm_events(id, &records->fsevents_iterator);
        break;
    case CL_TRUNC:
        statx_enrich_mask = RBH_STATX_CTIME_SEC | RBH_STATX_CTIME_NSEC |
                            RBH_STATX_MTIME_SEC | RBH_STATX_MTIME_NSEC |
                            RBH_STATX_SIZE;
        rc = build_statx_update_event(statx_enrich_mask, id,
                                      &records->fsevents_iterator);
        break;
    case CL_LAYOUT:
        rc = build_layout_events(id, &records->fsevents_iterator);
        break;
    case CL_FLRW:
        rc = build_flrw_events(id, &records->fsevents_iterator);
        break;
    case CL_RESYNC:
        rc = build_resync_events(id, &records->fsevents_iterator);
        break;
    case CL_MIGRATE:
        rc = build_migrate_events(record, id, &records->fsevents_iterator);
        break;
    default:
        /* These corespond to:
         * - CL_MARK: used by Lustre internals for llog management
         * - CL_EXT: unused in Lustre source code
         * - CL_OPEN: never activated as it would generate too many changelogs
         * - CL_XATTR: deprecated name
         * - CL_GETXATTR: never activated, does not modify anything
         * - CL_DN_OPEN: same reason
         *
         * So we do not manage these events and skip to the next changelog.
         */
        llapi_changelog_free(&record);
        goto retry;
    }

end_event:
    save_errno = errno;
    llapi_changelog_free(&record);
    errno = save_errno;

    if (rc != -1 && records->fsevents_iterator)
        return rbh_iter_next(records->fsevents_iterator);

    return NULL;
}

static int
lustre_changelog_set_last_read(void *iterator)
{
    struct rbh_value_pair last_read_pair, mdt_pair, fsevents_pair;
    struct lustre_changelog_iterator *records = iterator;
    struct rbh_value *last_read_value;
    struct rbh_value *map, *mdt_map;
    struct rbh_value_map value_map;
    int rc = 0;

    last_read_value = rbh_value_uint64_new(records->last_changelog_index);

    last_read_pair.key = "last_read";
    last_read_pair.value = last_read_value;

    mdt_map = rbh_value_map_new(&last_read_pair, 1);

    mdt_pair.key = records->mdt_name;
    mdt_pair.value = mdt_map;

    map = rbh_value_map_new(&mdt_pair, 1);

    fsevents_pair.key = "fsevents_source";
    fsevents_pair.value = map;
    value_map.pairs = &fsevents_pair;
    value_map.count = 1;

    rc = sink_insert_metadata(records->sink, &value_map, RBH_DT_INFO);

    free(last_read_value);
    free(mdt_map);
    free(map);

    return errno != ENOTSUP ? rc : 0;
}

void
lustre_changelog_iter_destroy(void *iterator)
{
    struct lustre_changelog_iterator *records = iterator;

    llapi_changelog_fini(&records->reader);

    if (records->fsevents_iterator)
        rbh_iter_destroy(records->fsevents_iterator);

    if (records->username) {
        int rc;

        rc = lustre_changelog_set_last_read(records);
        if (rc < 0)
            error(EXIT_FAILURE, -rc, "Failed to set backend_fsevents info");

        free(records->username);
    }

    free(records->mdt_name);
    if (records->dump_file != NULL && records->dump_file != stdout)
        fclose(records->dump_file);
}
