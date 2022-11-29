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

#include <robinhood/fsevent.h>
#include <robinhood/sstack.h>
#include <robinhood/statx.h>

#include "source.h"

static __thread struct rbh_sstack *_values;

static void
_values_flush(struct rbh_sstack *values)
{
    while (true) {
        size_t readable;

        rbh_sstack_peek(values, &readable);
        if (readable == 0)
            break;

        rbh_sstack_pop(values, readable);
    }
    rbh_sstack_shrink(values);
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

    bad = malloc(sizeof(*bad));
    if (bad == NULL)
        return NULL;

    bad->type = -1;
    bad->id.data = NULL;
    bad->id.size = 0;

    return bad;
}


static struct rbh_value_pair FID_XATTRS_PAIRS[] = {
    { .key = "fid" },
 };

static const struct rbh_value_map FID_XATTRS_MAP = {
    .pairs = FID_XATTRS_PAIRS,
    .count = 1,
};

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
static int
fill_ns_xattrs_fid(struct changelog_rec *record, struct rbh_value_pair *pair)
{
    struct rbh_value lu_fid_value;

    lu_fid_value.type = RBH_VT_BINARY;
    lu_fid_value.binary.size = sizeof(record->cr_tfid);
    lu_fid_value.binary.data = rbh_sstack_push(_values,
                                               (const char *)&record->cr_tfid,
                                               sizeof(record->cr_tfid));
    if (lu_fid_value.binary.data == NULL)
        return -1;

    pair->key = "fid";
    pair->value = rbh_sstack_push(_values, &lu_fid_value, sizeof(lu_fid_value));
    if (pair->value == NULL)
        return -1;

    return 0;
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

    mask = rbh_sstack_push(_values, NULL, sizeof(*mask));
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

    xattr_value = rbh_sstack_push(_values, NULL, sizeof(*xattr_value));
    if (xattr_value == NULL)
        return NULL;
    xattr_value->type = RBH_VT_STRING;
    xattr_value->string = rbh_sstack_push(_values, xattr_name,
                                          strlen(xattr_name) + 1);
    if (xattr_value->string == NULL)
        return NULL;
    xattr_sequence = rbh_sstack_push(_values, NULL, sizeof(*xattr_sequence));
    if (xattr_sequence == NULL)
        return NULL;
    xattr_sequence->type = RBH_VT_SEQUENCE;
    xattr_sequence->sequence.count = 1;
    xattr_sequence->sequence.values = xattr_value;

    return xattr_sequence;
}

static struct rbh_value_pair *
build_pair(const char *key, struct rbh_value *(*part_builder)(void *),
           void *part_builder_arg)
{
    const struct rbh_value_pair PAIR[] = {
        { .key = key, .value = part_builder(part_builder_arg) },
    };
    struct rbh_value_pair *pair;

    if (PAIR[0].value == NULL)
        return NULL;

    pair = rbh_sstack_push(_values, NULL, sizeof(*pair));
    if (pair == NULL)
        return NULL;
    memcpy(pair, PAIR, sizeof(*pair));

    return pair;
}

static struct rbh_value *
_fill_enrich(const char *key, struct rbh_value *(*builder)(void *),
             void *arg)
{
    const struct rbh_value_map MAP = {
        .count = 1,
        .pairs = build_pair(key, builder, arg),
    };
    const struct rbh_value ENRICH = {
        .type = RBH_VT_MAP,
        .map = MAP,
    };
    struct rbh_value *enrich;

    if (MAP.pairs == NULL)
        return NULL;

    enrich = rbh_sstack_push(_values, NULL, sizeof(*enrich));
    if (enrich == NULL)
        return NULL;
    memcpy(enrich, &ENRICH, sizeof(*enrich));

    return enrich;
}

/* BSON results:
 *  * { "xattrs" : { "rbh-fsevents" : { "xattrs" : [ a, b, c, ... ] } } }
 *   */
static struct rbh_value *
fill_inode_xattrs(void *arg)
{
    return _fill_enrich("xattrs", build_xattrs, arg);
}

static struct rbh_value *
fill_statx(void *arg)
{
    return _fill_enrich("statx", build_statx_mask, arg);
}

static struct rbh_value_map
build_enrich_map(struct rbh_value *(*part_builder)(void *),
                 void *part_builder_arg)
{
    const struct rbh_value_map ENRICH = {
        .count = 1,
        .pairs = build_pair("rbh-fsevents", part_builder, part_builder_arg),
    };

    return ENRICH;
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
    id = rbh_sstack_push(_values, NULL, sizeof(*id) + data_size);
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
build_create_event(unsigned int process_step, struct changelog_rec *record,
                   struct rbh_fsevent *fsevent)
{
    struct rbh_statx *rec_statx;
    uint32_t statx_enrich_mask;
    char *data;

    assert(process_step < 3);
    switch(process_step) {
        case 0:
            fsevent->type = RBH_FET_LINK;

            fsevent->xattrs = build_enrich_map(fill_inode_xattrs, "path");
            if (fsevent->xattrs.pairs == NULL)
                return -1;

            fsevent->link.parent_id = build_id(&record->cr_pfid);
            if (fsevent->link.parent_id == NULL)
                return -1;

            data = rbh_sstack_push(_values, NULL, record->cr_namelen + 1);
            if (data == NULL)
                return -1;
            memcpy(data, changelog_rec_name(record), record->cr_namelen);
            data[record->cr_namelen] = '\0';
            fsevent->link.name = data;

            return 1;
        case 1:
            fsevent->type = RBH_FET_XATTR;
            fsevent->xattrs = FID_XATTRS_MAP;
            if (fill_ns_xattrs_fid(record, &FID_XATTRS_PAIRS[0]))
                return -1;

            return 1;
        case 2:
            rec_statx = rbh_sstack_push(_values, NULL, sizeof(*rec_statx));
            if (rec_statx == NULL)
                return -1;

            rec_statx->stx_mask = 0;
            fill_uidgid(record, rec_statx);

            fsevent->type = RBH_FET_UPSERT;
            fsevent->upsert.statx = rec_statx;

            statx_enrich_mask = RBH_STATX_ALL;
            fsevent->xattrs = build_enrich_map(fill_statx,
                    &statx_enrich_mask);
            if (fsevent->xattrs.pairs == NULL)
                return -1;

            return 0;
    }
    __builtin_unreachable();
}

static int
build_setxattr_event(unsigned int process_step, struct changelog_rec *record,
                     struct rbh_fsevent *fsevent)
{
    char *xattr = changelog_rec_xattr(record)->cr_xattr;
    uint32_t statx_enrich_mask = 0;

    assert(process_step < 2);
    switch(process_step) {
    case 0:
        fsevent->type = RBH_FET_UPSERT;

        statx_enrich_mask = RBH_STATX_CTIME_SEC | RBH_STATX_CTIME_NSEC;
        fsevent->xattrs = build_enrich_map(fill_statx, &statx_enrich_mask);
        if (fsevent->xattrs.pairs == NULL)
            return -1;

        return 1;
    case 1:
        fsevent->type = RBH_FET_XATTR;
        fsevent->xattrs = build_enrich_map(fill_inode_xattrs, xattr);
        if (fsevent->xattrs.pairs == NULL)
            return -1;

        return 0;
    }
    __builtin_unreachable();
}

static const void *
lustre_changelog_iter_next(void *iterator)
{
    struct lustre_changelog_iterator *records = iterator;
    struct rbh_fsevent *fsevent = NULL;
    struct changelog_rec *record;
    struct rbh_id *id;
    int save_errno;
    int rc;

    _values_flush(_values);

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

    id = build_id(&record->cr_tfid);
    if (id == NULL)
        goto end_event;

    fsevent = rbh_sstack_push(_values, NULL, sizeof(*fsevent));
    if (fsevent == NULL)
        goto end_event;
    memset(fsevent, 0, sizeof(*fsevent));
    fsevent->id.data = id->data;
    fsevent->id.size = id->size;

    switch(record->cr_type) {
    case CL_CREATE:
        rc = build_create_event(records->process_step, record, fsevent);
        break;
    case CL_SETXATTR:
        rc = build_setxattr_event(records->process_step, record, fsevent);
        break;
    case CL_CLOSE:
    case CL_MKDIR:      /* RBH_FET_UPSERT */
    case CL_HARDLINK:   /* RBH_FET_LINK? */
    case CL_SOFTLINK:   /* RBH_FET_UPSERT + symlink */
    case CL_MKNOD:
    case CL_UNLINK:     /* RBH_FET_UNLINK or RBH_FET_DELETE */
    case CL_RMDIR:      /* RBH_FET_UNLINK or RBH_FET_DELETE */
    case CL_RENAME:     /* RBH_FET_UPSERT */
    case CL_EXT:
    case CL_OPEN:
    case CL_LAYOUT:
    case CL_TRUNC:
    case CL_SETATTR:    /* RBH_FET_XATTR? */
    case CL_HSM:
    case CL_MTIME:
    case CL_ATIME:
    case CL_CTIME:
    case CL_MIGRATE:
    case CL_FLRW:
    case CL_RESYNC:
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

    rbh_sstack_destroy(_values);
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
lustre_changelog_init(struct lustre_changelog_iterator *events,
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

    lustre_changelog_init(&source->events, mdtname);

    _values = rbh_sstack_new(sizeof(struct rbh_value_pair) * (1 << 7));
    source->events.prev_record = NULL;
    source->source = LUSTRE_SOURCE;
    return &source->source;
}
