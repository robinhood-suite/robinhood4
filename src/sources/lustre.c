/* SPDX-License-Identifier: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>

#include <lustre/lustreapi.h>

#include <robinhood/fsevent.h>

#include "source.h"

struct lustre_changelog_iterator {
    struct rbh_iterator iterator;

    void *reader;
};

static const void *
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

static const void *
lustre_changelog_iter_next(void *iterator)
{
    struct lustre_changelog_iterator *records = iterator;
    const struct rbh_fsevent *partial_event;
    struct changelog_rec *record;
    struct rbh_id *id = NULL;
    int save_errno;
    int rc;

retry:
    rc = llapi_changelog_recv(records->reader, &record);
    if (rc < 0) {
        errno = -rc;
        return NULL;
    }
    if (rc > 0) {
        errno = ENODATA;
        return NULL;
    }

    id = rbh_id_from_lu_fid(&record->cr_tfid);
    if (id == NULL) {
        save_errno = errno;
        llapi_changelog_free(&record);
        errno = save_errno;
        return NULL;
    }

    switch(record->cr_type) {
    case CL_CREATE:     /* RBH_FET_UPSERT */
        partial_event = rbh_fsevent_upsert_new(id, NULL, NULL, NULL);
        goto end_event;
    case CL_CLOSE:
        partial_event = rbh_fsevent_upsert_new(id, NULL, NULL, NULL);
        goto end_event;
    case CL_MKDIR:      /* RBH_FET_UPSERT */
        partial_event = rbh_fsevent_upsert_new(id, NULL, NULL, NULL);
        goto end_event;
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
    case CL_SETXATTR:   /* RBH_FET_XATTR */
    case CL_HSM:
    case CL_MTIME:
    case CL_ATIME:
    case CL_CTIME:
    case CL_MIGRATE:
    case CL_FLRW:
    case CL_RESYNC:
    case CL_GETXATTR:
    case CL_DN_OPEN:
        partial_event = fsevent_from_record(record);
        goto end_event;
    default: /* CL_MARK or other events */
        /* Events not managed yet, we go to the next record */
        llapi_changelog_free(&record);
        goto retry;
    }

    __builtin_unreachable();

end_event:
    save_errno = errno;
    llapi_changelog_free(&record);
    errno = save_errno;

    return partial_event;
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

    source->source = LUSTRE_SOURCE;
    return &source->source;
}
