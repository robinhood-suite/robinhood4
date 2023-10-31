/* SPDX-License-Identifier: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>

#include <lustre/lustreapi.h>

#include <robinhood/itertools.h>
#include <robinhood/fsevent.h>
#include <robinhood/sstack.h>
#include <robinhood/statx.h>

#include "source.h"
#include "utils.h"

static __thread struct rbh_iterator *fsevents_iterator;

static void __attribute__((destructor))
free_fsevents_iterator(void)
{
    if (fsevents_iterator)
        rbh_iter_destroy(fsevents_iterator);
}

struct lustre_changelog_iterator {
    struct rbh_iterator iterator;

    void *reader;
    struct changelog_rec *prev_record;
    unsigned int process_step;
};

static void *
fsevent_from_record(struct changelog_rec *record)
{
    struct rbh_fsevent *bad;

    (void)record;

    bad = source_stack_alloc(NULL, sizeof(*bad));
    if (bad == NULL)
        return NULL;

    bad->type = -1;
    bad->id.data = NULL;
    bad->id.size = 0;

    return bad;
}

/* BSON results:
 * { "statx" : { "uid" : x, "gid" : y } }
 */
static void
fill_uidgid(struct changelog_rec *record, struct rbh_statx *statx)
{
    struct changelog_ext_uidgid *uidgid;

    statx->stx_mask |= RBH_STATX_UID | RBH_STATX_GID;
    uidgid = changelog_rec_uidgid(record);
    statx->stx_uid = uidgid->cr_uid;
    statx->stx_gid = uidgid->cr_gid;
}

/* BSON results:
 * { "ns" : [ { "xattrs": { "fid" : x } } ] }
 */
static struct rbh_value *
fill_ns_xattrs_fid(void *arg)
{
    struct changelog_rec *record = (struct changelog_rec *)arg;
    struct rbh_value lu_fid_value;
    struct rbh_value *value;

    lu_fid_value.type = RBH_VT_BINARY;
    lu_fid_value.binary.size = sizeof(record->cr_tfid);
    lu_fid_value.binary.data = source_stack_alloc(
        (const char *)&record->cr_tfid, sizeof(record->cr_tfid));
    if (lu_fid_value.binary.data == NULL)
        return NULL;

    value = source_stack_alloc(&lu_fid_value, sizeof(lu_fid_value));
    if (value == NULL)
        return NULL;

    return value;
}

static struct rbh_value *
build_statx_mask(void *arg)
{
    uint32_t enrich_mask = *(uint32_t *)arg;
    const struct rbh_value MASK = {
        .type = RBH_VT_UINT32,
        .uint32 = enrich_mask,
    };
    struct rbh_value *mask;

    mask = source_stack_alloc(NULL, sizeof(*mask));
    if (mask == NULL)
        return NULL;
    *mask = MASK;

    return mask;
}

static struct rbh_value *
build_xattrs(void *arg)
{
    struct rbh_value *xattr_sequence;
    char *xattr_name = (char *)arg;
    struct rbh_value *xattr_value;

    xattr_value = source_stack_alloc(NULL, sizeof(*xattr_value));
    if (xattr_value == NULL)
        return NULL;

    xattr_value->type = RBH_VT_STRING;
    xattr_value->string = source_stack_alloc(xattr_name,
                                             strlen(xattr_name) + 1);
    if (xattr_value->string == NULL)
        return NULL;

    xattr_sequence = source_stack_alloc(NULL, sizeof(*xattr_sequence));
    if (xattr_sequence == NULL)
        return NULL;

    xattr_sequence->type = RBH_VT_SEQUENCE;
    xattr_sequence->sequence.count = 1;
    xattr_sequence->sequence.values = xattr_value;

    return xattr_sequence;
}

static struct rbh_value *
build_symlink_string(void *arg)
{
    const struct rbh_value SYMLINK = {
        .type = RBH_VT_STRING,
        .string = "symlink",
    };

    (void) arg;

    return source_stack_alloc(&SYMLINK, sizeof(SYMLINK));
}

static struct rbh_value *
_fill_enrich(const char *key, struct rbh_value *(*builder)(void *),
             void *arg)
{
    const struct rbh_value ENRICH = {
        .type = RBH_VT_MAP,
        .map = {
            .count = 1,
            .pairs = build_pair(key, builder, arg),
        },
    };
    struct rbh_value *enrich;

    if (ENRICH.map.pairs == NULL)
        return NULL;

    enrich = source_stack_alloc(NULL, sizeof(*enrich));
    if (enrich == NULL)
        return NULL;
    memcpy(enrich, &ENRICH, sizeof(*enrich));

    return enrich;
}

/* BSON results:
 * { "xattrs" : { "rbh-fsevents" : { "xattrs" : [ a, b, c, ... ] } } }
 */
static struct rbh_value *
fill_inode_xattrs(void *arg)
{
    return _fill_enrich("xattrs", build_xattrs, arg);
}

/* BSON results:
 * { "xattrs" : { "rbh-fsevents" : { "statx" : 1234567 } } }
 */
static struct rbh_value *
fill_statx(void *arg)
{
    return _fill_enrich("statx", build_statx_mask, arg);
}

/* BSON results:
 * { "xattrs" : { "rbh-fsevents" : { "symlink" : "symlink" } } }
 */
static struct rbh_value *
build_symlink_enrich_map(void *arg)
{
    return _fill_enrich("symlink", build_symlink_string, arg);
}

/* The variadic arguments must be given by pairs -a string key and a rbh value-,
 * and finished by a NULL pointer.
 */
static int
build_enrich_xattr_fsevent(struct rbh_value_map *xattrs_map,
                           char *key, struct rbh_value *value, ...)
{
    struct rbh_value_pair *pairs;
    va_list args;
    size_t count;
    size_t i;

    /* Loop to retrieve how many pairs we got, for the allocation.
     * The last argument must be NULL.
     */
    va_start(args, value);
    for (count = 0; va_arg(args, void *) != NULL; ++count);
    va_end(args);

    assert(count % 2 == 0); /* count must be even */
    xattrs_map->count = 1 + count / 2;

    pairs = rbh_sstack_push(
        source_stack, NULL, xattrs_map->count * sizeof(*xattrs_map->pairs));
    if (pairs == NULL)
        return -1;

    pairs[0].key = key;
    pairs[0].value = value;

    va_start(args, value);
    for (i = 1; i < xattrs_map->count; ++i) {
        pairs[i].key = va_arg(args, char *);
        /* value cannot be NULL here, or it will be catch by the assert above */
        pairs[i].value = va_arg(args, struct rbh_value *);
    }
    va_end(args);

    xattrs_map->pairs = pairs;

    return 0;
}

static struct rbh_id *
build_id(const struct lu_fid *fid)
{
    struct rbh_id *tmp_id;
    struct rbh_id *id;
    size_t data_size;
    char *data;
    int rc;

    tmp_id = rbh_id_from_lu_fid(fid);
    if (tmp_id == NULL)
        return NULL;

    data_size = tmp_id->size;
    id = source_stack_alloc(NULL, sizeof(*id) + data_size);
    if (id == NULL)
        return NULL;

    data = (char *)(id + 1);
    rc = rbh_id_copy(id, tmp_id, &data, &data_size);
    if (rc)
        return NULL;

    free(tmp_id);

    return id;
}

static int
create_statx_uid_gid(struct changelog_rec *record, struct rbh_statx **rec_statx)
{
    *rec_statx = source_stack_alloc(NULL, sizeof(**rec_statx));
    if (*rec_statx == NULL)
        return -1;

    (*rec_statx)->stx_mask = 0;
    (*rec_statx)->stx_mode = 0;
    fill_uidgid(record, *rec_statx);

    return 0;
}

static int
build_statx_event(uint32_t statx_enrich_mask, struct rbh_fsevent *fsevent,
                  struct rbh_statx *rec_statx)
{
    fsevent->type = RBH_FET_UPSERT;
    fsevent->upsert.statx = rec_statx;

    fsevent->xattrs = build_enrich_map(fill_statx, &statx_enrich_mask);
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

    statx_enrich_mask = RBH_STATX_ATIME | RBH_STATX_CTIME | RBH_STATX_MTIME;
    if (build_statx_event(statx_enrich_mask, fsevent, NULL))
        return -1;

    return 0;
}

/* Initialize a batch of \p n_events fsevents, and copy a given rbh_id \p id
 * to each event in the batch.
 */
static void
initialize_events(struct rbh_fsevent **_new_events, size_t n_events,
                  struct rbh_id *id)
{
    struct rbh_fsevent *new_events;
    size_t i;

    new_events = source_stack_alloc(NULL, sizeof(*new_events) * n_events);
    if (new_events == NULL)
        error(EXIT_FAILURE, errno,
              "source_stack_alloc in initialize_events for '%lu' events",
              n_events);

    memset(new_events, 0, sizeof(*new_events) * n_events);

    for (i = 0; i < n_events; ++i) {
        new_events[i].id.data = source_stack_alloc(id->data, id->size);
        if (!new_events[i].id.data)
            error(EXIT_FAILURE, errno,
                  "source_stack_alloc in initialize_events for id copy");

        new_events[i].id.size = id->size;
    }

    *_new_events = new_events;
}

static int
build_create_inode_events(struct changelog_rec *record, struct rbh_id *id)
{
    struct rbh_fsevent *new_events;

    initialize_events(&new_events, 4, id);

    if (new_link_inode_event(record, &new_events[0]))
        return -1;

    new_events[1].type = RBH_FET_XATTR;
    if (build_enrich_xattr_fsevent(&new_events[1].xattrs,
                                   "fid",
                                   fill_ns_xattrs_fid(record),
                                   "rbh-fsevents",
                                   build_empty_map("lustre"),
                                   NULL))
        return -1;

    if (update_statx_without_uid_gid_event(record, &new_events[2]))
        return -1;

    /* Update the parent information after creating a new entry */
    if (update_parent_acmtime_event(&record->cr_pfid, &new_events[3]))
        return -1;

    fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 4);

    return 0;
}

static int
build_setxattr_event(struct changelog_rec *record, struct rbh_id *id)
{
    char *xattr = changelog_rec_xattr(record)->cr_xattr;
    struct rbh_fsevent *new_events;
    uint32_t statx_enrich_mask = 0;

    initialize_events(&new_events, 2, id);

    new_events[0].type = RBH_FET_UPSERT;
    statx_enrich_mask = RBH_STATX_CTIME_SEC | RBH_STATX_CTIME_NSEC;
    new_events[0].xattrs = build_enrich_map(fill_statx, &statx_enrich_mask);
    if (new_events[0].xattrs.pairs == NULL)
        return -1;

    new_events[1].type = RBH_FET_XATTR;
    new_events[1].xattrs = build_enrich_map(fill_inode_xattrs, xattr);
    if (new_events[1].xattrs.pairs == NULL)
        return -1;

    fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 2);

    return 0;
}

static int
build_statx_update_event(uint32_t statx_enrich_mask, struct rbh_id *id)
{
    struct rbh_fsevent *new_events;

    initialize_events(&new_events, 1, id);

    new_events[0].type = RBH_FET_UPSERT;
    new_events[0].xattrs = build_enrich_map(fill_statx, &statx_enrich_mask);
    if (new_events[0].xattrs.pairs == NULL)
        return -1;

    fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 1);

    return 0;
}

/* Do the exact same operations as for creating an inode, except for an
 * additional one that is the enrichment of the symlink target
 */
static int
build_softlink_events(struct changelog_rec *record, struct rbh_id *id)
{
    struct rbh_fsevent *new_events;

    initialize_events(&new_events, 5, id);

    if (new_link_inode_event(record, &new_events[0]))
        return -1;

    new_events[1].type = RBH_FET_XATTR;
    if (build_enrich_xattr_fsevent(&new_events[1].xattrs,
                                   "fid",
                                   fill_ns_xattrs_fid(record),
                                   NULL))
        return -1;

    if (update_statx_without_uid_gid_event(record, &new_events[2]))
        return -1;

    /* Update the parent information after creating a new entry */
    if (update_parent_acmtime_event(&record->cr_pfid, &new_events[3]))
        return -1;

    /* Mark the event for enrichment of the symlink target */
    new_events[4].type = RBH_FET_UPSERT;
    new_events[4].xattrs = build_enrich_map(build_symlink_enrich_map, NULL);
    if (new_events[4].xattrs.pairs == NULL)
        return -1;

    fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 5);

    return 0;
}

static int
build_hardlink_or_mknod_events(struct changelog_rec *record, struct rbh_id *id)
{
    struct rbh_fsevent *new_events;

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
    initialize_events(&new_events, 4, id);

    if (new_link_inode_event(record, &new_events[0]))
        return -1;

    if (update_statx_without_uid_gid_event(record, &new_events[1]))
        return -1;

    /* Update the parent information after creating a new entry */
    if (update_parent_acmtime_event(&record->cr_pfid, &new_events[2]))
        return -1;

    new_events[3].type = RBH_FET_XATTR;
    if (build_enrich_xattr_fsevent(&new_events[3].xattrs,
                                   "rbh-fsevents",
                                   build_empty_map("lustre"),
                                   NULL))
        return -1;

    fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 4);

    return 0;
}

static int
unlink_inode_event(struct lu_fid *parent_id, char *name, size_t namelen,
                   bool last_copy, struct rbh_fsevent *fsevent)
{
    char *data;

    fsevent->xattrs.count = 0;

    /* If the unlinked target is the last link and it has no copy archived,
     * delete the entry altogether.
     */
    if (last_copy) {
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
build_unlink_or_rmdir_events(struct changelog_rec *record, struct rbh_id *id)
{
    bool last_copy = (record->cr_flags & CLF_UNLINK_LAST) &&
                     !(record->cr_flags & CLF_UNLINK_HSM_EXISTS);
    struct rbh_fsevent *new_events;

    initialize_events(&new_events, 2, id);

    if (unlink_inode_event(&record->cr_pfid, changelog_rec_name(record),
                           record->cr_namelen, last_copy, &new_events[0]))
        return -1;

    if (update_parent_acmtime_event(&record->cr_pfid, &new_events[1]))
        return -1;

    fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 2);

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
build_rename_events(struct changelog_rec *record, struct rbh_id *id)
{
    struct changelog_ext_rename *rename_log = changelog_rec_rename(record);
    /* If the overwriten link is the last one and it has no HSM copy */
    bool last_copy = (record->cr_flags & CLF_RENAME_LAST) &&
                     !(record->cr_flags & CLF_RENAME_LAST_EXISTS);
    struct rbh_fsevent *new_events;
    struct rbh_id *renamed_id;
    int counter = 0;
    int n_events;

    /* Unlink of the overwriten link is the first step done, but if the target
     * fid is 0, nothing was overwritten, so we don't need a corresponding
     * event.
     */
    if (fid_is_zero(&record->cr_tfid))
        n_events = 5;
    else
        n_events = 6;

    renamed_id = build_id(&rename_log->cr_sfid);
    if (renamed_id == NULL)
        return -1;

    /* Create the batch of events and give them the ID of the renamed entry */
    initialize_events(&new_events, n_events, renamed_id);

    /* If an entry is overwritten, simply change the target ID of the unlink to
     * that one.
     */
    if (!fid_is_zero(&record->cr_tfid)) {
        new_events[counter].id.data = id->data;
        new_events[counter].id.size = id->size;

        if (unlink_inode_event(&record->cr_pfid, changelog_rec_name(record),
                               record->cr_namelen, last_copy,
                               &new_events[counter]))
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

    counter ++;

    if (unlink_inode_event(&rename_log->cr_spfid, changelog_rec_sname(record),
                           changelog_rec_snamelen(record),
                           false, &new_events[counter]))
        return -1;

    counter++;

    if (update_parent_acmtime_event(&rename_log->cr_spfid,
                                    &new_events[counter]))
        return -1;

    fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events),
                                       n_events);

    return 0;
}

/* In the future we will need to modify this function to create two events for
 * releases and restores, because they both modify the layout of the file as
 * well as the hsm state. Currently, these operations both trigger a CL_LAYOUT
 * changelog, so the layout modification will be managed in that event's
 * associated function.
 */
static int
build_hsm_events(struct rbh_id *id)
{
    uint32_t statx_enrich_mask = RBH_STATX_BLOCKS;
    struct rbh_fsevent *new_events;

    initialize_events(&new_events, 4, id);

    new_events[0].type = RBH_FET_UPSERT;
    new_events[0].xattrs = build_enrich_map(fill_statx, &statx_enrich_mask);
    if (new_events[0].xattrs.pairs == NULL)
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

    fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 4);

    return 0;
}

static int
build_layout_events(struct rbh_id *id)
{
    uint32_t statx_enrich_mask = RBH_STATX_CTIME_SEC | RBH_STATX_CTIME_NSEC;
    struct rbh_fsevent *new_events;

    initialize_events(&new_events, 2, id);

    new_events[0].type = RBH_FET_UPSERT;
    new_events[0].xattrs = build_enrich_map(fill_statx, &statx_enrich_mask);
    if (new_events[0].xattrs.pairs == NULL)
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

    fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 2);

    return 0;
}

/* FLRW events are events that happen when writing data to a mirrored file in
 * Lustre. When doing so, it will write data to the "main" copy, and set a
 * "dirty" flag to the other copies signifying a synchronization is necessary.
 * As such, managing this event is the same as managing a truncate + retrieving
 * the Lustre information for that file.
 */
static int
build_flrw_events(struct rbh_id *id)
{
    uint32_t statx_enrich_mask = RBH_STATX_CTIME_SEC | RBH_STATX_CTIME_NSEC |
                                 RBH_STATX_MTIME_SEC | RBH_STATX_MTIME_NSEC |
                                 RBH_STATX_BLOCKS | RBH_STATX_SIZE;
    struct rbh_fsevent *new_events;

    initialize_events(&new_events, 2, id);

    if (build_statx_event(statx_enrich_mask, &new_events[0], NULL))
        return -1;

    new_events[1].type = RBH_FET_XATTR;
    if (build_enrich_xattr_fsevent(&new_events[1].xattrs,
                                   "rbh-fsevents", build_empty_map("lustre"),
                                   NULL))
        return -1;

    fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 2);

    return 0;
}

/* Resync events are events that happen when synchronizing a mirrored file in
 * Lustre. When doing so, all mirrors of a file will copy the data from the
 * "main" mirror, and remove the "dirty" flags from the other copies. As such,
 * managing this event is the same as managing a FLRW event, minus a few statx
 * values.
 */
static int
build_resync_events(struct rbh_id *id)
{
    uint32_t statx_enrich_mask = RBH_STATX_CTIME_SEC | RBH_STATX_CTIME_NSEC |
                                 RBH_STATX_BLOCKS;
    struct rbh_fsevent *new_events;

    initialize_events(&new_events, 2, id);

    if (build_statx_event(statx_enrich_mask, &new_events[0], NULL))
        return -1;

    new_events[1].type = RBH_FET_XATTR;
    if (build_enrich_xattr_fsevent(&new_events[1].xattrs,
                                   "rbh-fsevents", build_empty_map("lustre"),
                                   NULL))
        return -1;

    fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 2);

    return 0;
}

/* Migrate events only correspond to metadata changes, meaning we only have to
 * change the target and target's parent striping information.
 */
static int
build_migrate_events(struct changelog_rec *record, struct rbh_id *id)
{
    struct changelog_ext_rename *migrate_log = changelog_rec_rename(record);
    struct rbh_fsevent *new_events;
    struct rbh_id *migrated_id;

    migrated_id = build_id(&migrate_log->cr_sfid);
    if (migrated_id == NULL)
        return -1;

    initialize_events(&new_events, 6, id);

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
                           &new_events[3]))
        return -1;

    if (update_parent_acmtime_event(&migrate_log->cr_spfid, &new_events[4]))
        return -1;

    new_events[5].type = RBH_FET_XATTR;
    if (build_enrich_xattr_fsevent(&new_events[5].xattrs,
                                   "rbh-fsevents", build_empty_map("lustre"),
                                   NULL))
        return -1;

    fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 6);

    return 0;
}

static const void *
lustre_changelog_iter_next(void *iterator)
{
    struct lustre_changelog_iterator *records = iterator;
    const struct rbh_fsevent *to_return;
    struct rbh_fsevent *fsevent = NULL;
    uint32_t statx_enrich_mask = 0;
    struct changelog_rec *record;
    struct rbh_id *id;
    int save_errno;
    int rc;

    flush_source_stack();

    if (fsevents_iterator) {
        to_return = rbh_iter_next(fsevents_iterator);
        if (to_return != NULL)
            return to_return;

        rbh_iter_destroy(fsevents_iterator);
        fsevents_iterator = NULL;
    }

retry:
    if (records->prev_record == NULL) {
        rc = llapi_changelog_recv(records->reader, &record);
        if (rc < 0) {
            errno = -rc;
            return NULL;
        }
        if (rc > 0) {
            errno = ENODATA;
            return NULL;
        }
        records->process_step = 0;
    } else {
        record = records->prev_record;
        records->prev_record = NULL;
    }

    fsevent = source_stack_alloc(NULL, sizeof(*fsevent));
    if (fsevent == NULL)
        goto end_event;
    memset(fsevent, 0, sizeof(*fsevent));

    id = build_id(&record->cr_tfid);
    if (id == NULL)
        goto err;

    fsevent->id.data = id->data;
    fsevent->id.size = id->size;

    switch (record->cr_type) {
    case CL_CREATE:
    case CL_MKDIR:
        rc = build_create_inode_events(record, id);
        break;
    case CL_SETXATTR:
        rc = build_setxattr_event(record, id);
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
        rc = build_statx_update_event(statx_enrich_mask, id);
        break;
    case CL_SOFTLINK:
        rc = build_softlink_events(record, id);
        break;
    case CL_HARDLINK:
    case CL_MKNOD:
        rc = build_hardlink_or_mknod_events(record, id);
        break;
    case CL_RMDIR:
    case CL_UNLINK:
        rc = build_unlink_or_rmdir_events(record, id);
        break;
    case CL_RENAME:
        rc = build_rename_events(record, id);
        break;
    case CL_HSM:
        rc = build_hsm_events(id);
        break;
    case CL_TRUNC:
        statx_enrich_mask = RBH_STATX_CTIME_SEC | RBH_STATX_CTIME_NSEC |
                            RBH_STATX_MTIME_SEC | RBH_STATX_MTIME_NSEC |
                            RBH_STATX_SIZE;
        rc = build_statx_update_event(statx_enrich_mask, id);
        break;
    case CL_LAYOUT:
        rc = build_layout_events(id);
        break;
    case CL_FLRW:
        rc = build_flrw_events(id);
        break;
    case CL_RESYNC:
        rc = build_resync_events(id);
        break;
    case CL_MIGRATE:
        rc = build_migrate_events(record, id);
        break;
    case CL_EXT:
    case CL_OPEN:
    case CL_GETXATTR:
    case CL_DN_OPEN:
        fsevent = fsevent_from_record(record);
        goto end_event;
    default: /* CL_MARK or other events */
        /* Events not managed yet, we go to the next record */
        llapi_changelog_free(&record);
        goto retry;
    }

    if (rc == -1)
        goto err;
    else if (rc == 1) /* record is partially parsed */
        goto save_event;
    else /* record is fully parsed */
        goto end_event;

err:
    fsevent = NULL;

end_event:
    save_errno = errno;
    llapi_changelog_free(&record);
    errno = save_errno;

    if (fsevents_iterator)
        return rbh_iter_next(fsevents_iterator);

    return fsevent;

save_event:
    records->prev_record = record;
    ++records->process_step;

    return fsevent;
}

static void
lustre_changelog_iter_destroy(void *iterator)
{
    struct lustre_changelog_iterator *records = iterator;

    llapi_changelog_fini(&records->reader);
}

static const struct rbh_iterator_operations LUSTRE_CHANGELOG_ITER_OPS = {
    .next = lustre_changelog_iter_next,
    .destroy = lustre_changelog_iter_destroy,
};

static const struct rbh_iterator LUSTRE_CHANGELOG_ITERATOR = {
    .ops = &LUSTRE_CHANGELOG_ITER_OPS,
};

static void
lustre_changelog_iter_init(struct lustre_changelog_iterator *events,
                           const char *mdtname)
{
    int rc;

    rc = llapi_changelog_start(&events->reader,
                               CHANGELOG_FLAG_JOBID |
                               CHANGELOG_FLAG_EXTRA_FLAGS,
                               mdtname, 0 /*start_rec*/);
    if (rc < 0)
        error(EXIT_FAILURE, -rc, "llapi_changelog_start");

    rc = llapi_changelog_set_xflags(events->reader,
                                    CHANGELOG_EXTRA_FLAG_UIDGID |
                                    CHANGELOG_EXTRA_FLAG_NID |
                                    CHANGELOG_EXTRA_FLAG_OMODE |
                                    CHANGELOG_EXTRA_FLAG_XATTR);
    if (rc < 0)
        error(EXIT_FAILURE, -rc, "llapi_changelog_set_xflags");

    events->iterator = LUSTRE_CHANGELOG_ITERATOR;
}

struct lustre_source {
    struct source source;

    struct lustre_changelog_iterator events;
};

static const void *
source_iter_next(void *iterator)
{
    struct lustre_source *source = iterator;

    return rbh_iter_next(&source->events.iterator);
}

static void
source_iter_destroy(void *iterator)
{
    struct lustre_source *source = iterator;

    rbh_iter_destroy(&source->events.iterator);
    free(source);
}

static const struct rbh_iterator_operations SOURCE_ITER_OPS = {
    .next = source_iter_next,
    .destroy = source_iter_destroy,
};

static const struct source LUSTRE_SOURCE = {
    .name = "lustre",
    .fsevents = {
        .ops = &SOURCE_ITER_OPS,
    },
};

struct source *
source_from_lustre_changelog(const char *mdtname)
{
    struct lustre_source *source;

    source = malloc(sizeof(*source));
    if (source == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    lustre_changelog_iter_init(&source->events, mdtname);

    initialize_source_stack(sizeof(struct rbh_value_pair) * (1 << 7));
    source->events.prev_record = NULL;
    source->source = LUSTRE_SOURCE;
    return &source->source;
}
