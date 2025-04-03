/* SPDX-License-Identifier: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>

#include <lustre/lustreapi.h>
#include <linux/lustre/lustre_fid.h>

#include <robinhood/itertools.h>
#include <robinhood/fsevent.h>
#include <robinhood/sstack.h>
#include <robinhood/statx.h>
#include <robinhood/config.h>
#include <robinhood/utils.h>

#include "source.h"
#include "utils.h"

struct lustre_changelog_iterator {
    struct rbh_iterator iterator;

    void *reader;
    struct rbh_iterator *fsevents_iterator;

    char *username;
    char *mdt_name;
    int32_t source_mdt_index;
    uint64_t last_changelog_index;

    FILE *dump_file;
};

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

static const char *
get_event_name(unsigned int cl_event)
{
    static const char * const event_name[] = {
        "archive", "restore", "cancel", "release", "remove", "state",
    };

    if (cl_event >= (sizeof(event_name) / sizeof(event_name[0])))
        return "unknown";
    else
        return event_name[cl_event];
}

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

/* Dump a single record. */
static void
dump_changelog(struct lustre_changelog_iterator *records,
               struct changelog_rec *record)
{
    char record_str[RBH_PATH_MAX] = "";
    int left = sizeof(record_str);
    char flag_buff[256] = "";
    char *curr = record_str;
    int len = 0;

    if (record->cr_type == CL_HSM)
        sprintf(flag_buff, "(%s%s,rc=%d)",
                get_event_name(hsm_get_cl_event(record->cr_flags)),
                hsm_get_cl_flags(record->cr_flags) & CLF_HSM_DIRTY ?
                    ",dirty" : "",
                hsm_get_cl_error(record->cr_flags));

    len = snprintf(curr, left, CL_BASE_FORMAT,
                   CL_BASE_ARG(records->mdt_name, record));
    curr += len;
    left -= len;

    if (left > 0 && record->cr_namelen) {
        /* this record has a 'name' field. */
        len = snprintf(curr, left, " " CL_NAME_FORMAT, CL_NAME_ARG(record));
        curr += len;
        left -= len;
    }

    if (left <= 0) {
        record_str[RBH_PATH_MAX - 1] = '\0';
        goto dump;
    }

    if (record->cr_flags & CLF_RENAME) {
        struct changelog_ext_rename *cr_rename;

        cr_rename = changelog_rec_rename(record);
        if (fid_is_sane(&cr_rename->cr_sfid)) {
            len = snprintf(curr, left, " " CL_EXT_FORMAT,
                           PFID(&cr_rename->cr_sfid),
                           PFID(&cr_rename->cr_spfid),
                           (int)changelog_rec_snamelen(record),
                           changelog_rec_sname(record));
            curr += len;
            left -= len;
        }
    }

    if (record->cr_flags & CLF_JOBID) {
        struct changelog_ext_jobid *jobid;

        jobid = changelog_rec_jobid(record);
        if (jobid->cr_jobid[0] != '\0') {
            len = snprintf(curr, left, " J=%s", jobid->cr_jobid);
            curr += len;
            left -= len;
        }
    }

    if (left <= 0)
        record_str[RBH_PATH_MAX - 1] = '\0';

dump:
    fprintf(records->dump_file, "%s\n", record_str);
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
 * { "xattrs": { "fid" : x } }
 */
static struct rbh_value *
fill_xattrs_fid(void *arg)
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

/* BSON results:
 * { "xattrs": { "mdt_index" : x } }
 */
static struct rbh_value *
fill_xattrs_mdt_index(void *arg)
{
    int32_t mdt_index = *((int32_t *) arg);
    struct rbh_value mdt_index_value;
    struct rbh_value *value;

    mdt_index_value.type = RBH_VT_INT32;
    mdt_index_value.int32 = mdt_index;

    value = source_stack_alloc(&mdt_index_value, sizeof(mdt_index_value));
    if (value == NULL)
        return NULL;

    return value;
}

/* BSON results:
 * { "xattrs": { "nb_children" : x [+-]1 } }
 */
static struct rbh_value *
fill_xattrs_nb_children(void *arg)
{
    struct rbh_value nb_children_value;
    int64_t inc = *((int64_t *) arg);
    struct rbh_value *value;

    nb_children_value.type = RBH_VT_INT64;
    nb_children_value.int64 = inc;

    value = source_stack_alloc(&nb_children_value, sizeof(nb_children_value));
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

    pairs = source_stack_alloc(NULL,
                               xattrs_map->count * sizeof(*xattrs_map->pairs));
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

static struct rbh_value *
build_statx_lustre_map(void *enrich_mask)
{
    struct rbh_value_pair *enrich_values;
    struct rbh_value_pair *lustre_pair;
    struct rbh_value_pair *statx_pair;
    struct rbh_value_map map;
    struct rbh_value enrich;

    enrich_values = source_stack_alloc(NULL, sizeof(*enrich_values) * 2);

    statx_pair = build_pair("statx", build_statx_mask, enrich_mask);
    lustre_pair = build_pair("lustre", NULL, NULL);

    memcpy(&enrich_values[0], statx_pair, sizeof(*statx_pair));
    memcpy(&enrich_values[1], lustre_pair, sizeof(*lustre_pair));

    map.pairs = enrich_values;
    map.count = 2;

    enrich.map = map;
    enrich.type = RBH_VT_MAP;

    return source_stack_alloc(&enrich, sizeof(enrich));
}

static int
build_statx_event(uint32_t statx_enrich_mask, struct rbh_fsevent *fsevent,
                  struct rbh_statx *rec_statx)
{
    fsevent->type = RBH_FET_UPSERT;
    fsevent->upsert.statx = rec_statx;

    fsevent->xattrs = build_enrich_map(build_statx_lustre_map,
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

    statx_enrich_mask = RBH_STATX_ATIME | RBH_STATX_CTIME | RBH_STATX_MTIME;
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

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 5);

    return 0;
}

static int
build_setxattr_event(struct changelog_rec *record, struct rbh_id *id,
                     struct rbh_iterator **fsevents_iterator)
{
    char *xattr = changelog_rec_xattr(record)->cr_xattr;
    struct rbh_fsevent *new_events;
    uint32_t statx_enrich_mask = 0;

    new_events = fsevent_list_alloc(2, id);

    statx_enrich_mask = RBH_STATX_CTIME_SEC | RBH_STATX_CTIME_NSEC;
    if (build_statx_event(statx_enrich_mask, &new_events[0], NULL))
        return -1;

    new_events[1].type = RBH_FET_XATTR;
    new_events[1].xattrs = build_enrich_map(fill_inode_xattrs, xattr);
    if (new_events[1].xattrs.pairs == NULL)
        return -1;

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 2);

    return 0;
}

static int
build_statx_update_event(uint32_t statx_enrich_mask, struct rbh_id *id,
                         struct rbh_iterator **fsevents_iterator)
{
    struct rbh_fsevent *new_events;

    new_events = fsevent_list_alloc(1, id);

    if (build_statx_event(statx_enrich_mask, &new_events[0], NULL))
        return -1;

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 1);

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

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 6);

    return 0;
}

static int
build_hardlink_or_mknod_events(struct changelog_rec *record, struct rbh_id *id,
                               struct rbh_iterator **fsevents_iterator)
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
    new_events = fsevent_list_alloc(5, id);

    if (new_link_inode_event(record, &new_events[0]))
        return -1;

    if (update_statx_without_uid_gid_event(record, &new_events[1]))
        return -1;

    /* Update the parent information after creating a new entry */
    if (update_parent_acmtime_event(&record->cr_pfid, &new_events[2]))
        return -1;

    if (update_parent_nb_children_event(&record->cr_pfid, 1, &new_events[3]))
        return -1;

    new_events[3].type = RBH_FET_XATTR;
    if (build_enrich_xattr_fsevent(&new_events[4].xattrs,
                                   "rbh-fsevents",
                                   build_empty_map("lustre"),
                                   NULL))
        return -1;

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 5);

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
build_unlink_or_rmdir_events(struct changelog_rec *record, struct rbh_id *id,
                             struct rbh_iterator **fsevents_iterator)
{
    bool last_copy = (record->cr_flags & CLF_UNLINK_LAST) &&
                     !(record->cr_flags & CLF_UNLINK_HSM_EXISTS);
    struct rbh_fsevent *new_events;

    new_events = fsevent_list_alloc(3, id);

    if (unlink_inode_event(&record->cr_pfid, changelog_rec_name(record),
                           record->cr_namelen, last_copy, &new_events[0]))
        return -1;

    if (update_parent_acmtime_event(&record->cr_pfid, &new_events[1]))
        return -1;

    if (update_parent_nb_children_event(&record->cr_pfid, -1, &new_events[2]))
        return -1;

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 3);

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
    bool last_copy = (record->cr_flags & CLF_RENAME_LAST) &&
                     !(record->cr_flags & CLF_RENAME_LAST_EXISTS);
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

    counter++;

    if (fid_is_zero(&record->cr_tfid)) {
        if (update_parent_nb_children_event(&record->cr_pfid, 1,
                                            &new_events[counter]))
            return -1;

        counter++;
    }

    if (unlink_inode_event(&rename_log->cr_spfid, changelog_rec_sname(record),
                           changelog_rec_snamelen(record),
                           false, &new_events[counter]))
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

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 4);

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

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 2);

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

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 2);

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

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 2);

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
                           &new_events[3]))
        return -1;

    if (update_parent_acmtime_event(&migrate_log->cr_spfid, &new_events[4]))
        return -1;

    new_events[5].type = RBH_FET_XATTR;
    if (build_enrich_xattr_fsevent(&new_events[5].xattrs,
                                   "rbh-fsevents", build_empty_map("lustre"),
                                   NULL))
        return -1;

    *fsevents_iterator = rbh_iter_array(new_events, sizeof(*new_events), 6);

    return 0;
}

static const void *
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

retry:
    rc = llapi_changelog_recv(records->reader, &record);
    if (rc > 0 || rc == -EAGAIN) {
        errno = ENODATA;
        return NULL;
    } else if (rc < 0) {
        errno = -rc;
        return NULL;
    }

    records->last_changelog_index = record->cr_index;

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

static void
lustre_changelog_iter_destroy(void *iterator)
{
    struct lustre_changelog_iterator *records = iterator;

    llapi_changelog_fini(&records->reader);
    if (records->fsevents_iterator)
        rbh_iter_destroy(records->fsevents_iterator);

    if (records->username) {
        int rc;

        rc = llapi_changelog_clear(records->mdt_name, records->username,
                                   records->last_changelog_index);
        if (rc < 0)
            error(EXIT_FAILURE, -rc,
                  "Failed to acknowledge changelogs, username '%s' may be invalid",
                  records->username);

        free(records->username);
    }

    free(records->mdt_name);
    if (records->dump_file != NULL && records->dump_file != stdout)
        fclose(records->dump_file);
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
                           const char *mdtname, const char *username,
                           const char *dump_file)
{
    const char *mdtname_index;
    int rc;

    events->last_changelog_index = 0;

    rc = llapi_changelog_start(&events->reader,
                               CHANGELOG_FLAG_JOBID |
                               CHANGELOG_FLAG_EXTRA_FLAGS,
                               mdtname, events->last_changelog_index);
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
    events->fsevents_iterator = NULL;

    if (username != NULL) {
        events->username = strdup(username);
        if (events->username == NULL)
            error(EXIT_FAILURE, ENOMEM, "strdup");
    } else {
        events->username = NULL;
    }

    events->mdt_name = strdup(mdtname);
    if (events->mdt_name == NULL)
        error(EXIT_FAILURE, ENOMEM, "strdup");

    for (mdtname_index = mdtname; !isdigit(*mdtname_index); mdtname_index++);

    rc = str2int64_t(mdtname_index, (int64_t *) &events->source_mdt_index);
    if (rc)
        error(EXIT_FAILURE, errno, "str2int64_t");

    if (dump_file == NULL) {
        events->dump_file = NULL;
    } else if (strcmp(dump_file, "-") == 0) {
        events->dump_file = stdout;
    } else {
        events->dump_file = fopen(dump_file, "a");
        if (events->dump_file == NULL)
            error(EXIT_FAILURE, errno, "Failed to open the dump file");
    }
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
source_from_lustre_changelog(const char *mdtname, const char *username,
                             const char *dump_file)
{
    struct lustre_source *source;

    source = malloc(sizeof(*source));
    if (source == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    lustre_changelog_iter_init(&source->events, mdtname, username,
                               dump_file);

    initialize_source_stack(sizeof(struct rbh_value_pair) * (1 << 7));
    source->source = LUSTRE_SOURCE;
    return &source->source;
}
