/* This file is part of Robinhood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <dlfcn.h>
#include <errno.h>
#include <error.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include <robinhood.h>
#include <robinhood/utils.h>

#ifndef RBH_ITER_CHUNK_SIZE
# define RBH_ITER_CHUNK_SIZE (1 << 12)
#endif

static struct rbh_backend *from, *to;

static void __attribute__((destructor))
destroy_from(void)
{
    const char *name;

    if (from) {
        name = from->name;
        rbh_backend_destroy(from);
        rbh_backend_plugin_destroy(name);
    }
}

static void __attribute__((destructor))
destroy_to(void)
{
    const char *name;

    if (to) {
        name = to->name;
        rbh_backend_destroy(to);
        rbh_backend_plugin_destroy(name);
    }
}

static struct rbh_mut_iterator *chunks;

static void __attribute__((destructor))
destroy_chunks(void)
{
    if (chunks)
        rbh_mut_iter_destroy(chunks);
}

static bool one = false;
static bool skip_error = true;
/*----------------------------------------------------------------------------*
 |                                   sync()                                   |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                           mut_iter_one()                           |
     *--------------------------------------------------------------------*/

struct one_iterator {
    struct rbh_mut_iterator iterator;
    void *element;
};

static void *
one_mut_iter_next(void *iterator)
{
    struct one_iterator *one = iterator;

    if (one->element) {
        void *element = one->element;

        one->element = NULL;
        return element;
    }

    errno = ENODATA;
    return NULL;
}

static void
one_mut_iter_destroy(void *iterator)
{
    struct one_iterator *one = iterator;

    free(one->element);
    free(one);
}

static const struct rbh_mut_iterator_operations ONE_ITER_OPS = {
    .next = one_mut_iter_next,
    .destroy = one_mut_iter_destroy,
};

static const struct rbh_mut_iterator ONE_ITERATOR = {
    .ops = &ONE_ITER_OPS,
};

static struct rbh_mut_iterator *
mut_iter_one(void *element)
{
    struct one_iterator *one;

    one = malloc(sizeof(*one));
    if (one == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    one->element = element;
    one->iterator = ONE_ITERATOR;
    return &one->iterator;
}

    /*--------------------------------------------------------------------*
     |                           iter_convert()                           |
     *--------------------------------------------------------------------*/

/* A convert_iterator converts fsentries into fsevents.
 *
 * For each fsentry, it yields up to two fsevents (depending on the information
 * available in the fsentry): one RBH_FET_UPSERT, to create the inode in the
 * backend; and one RBH_FET_LINK to "link" the inode in the namespace.
 */
struct convert_iterator {
    struct rbh_iterator iterator;
    struct rbh_iterator *fsentries;

    const struct rbh_filter_projection *projection;
    const struct rbh_fsentry *fsentry;
    struct rbh_fsevent fsevent;
    struct rbh_statx statx;
    struct {
        bool upsert:1;
        bool inode_xattr:1;
        bool link:1;
        bool ns_xattr:1;
    } todo;
};

/* Advance a convert_iterator to its next fsentry */
static int
_convert_iter_next(struct convert_iterator *convert,
                   const struct rbh_filter_projection *projection)
{
    const struct rbh_fsentry *fsentry;
    bool upsert, inode_xattr, link, ns_xattr;

    do {
        struct {
            bool id:1;
            bool parent_id:1;
            bool name:1;
            bool inode_xattrs:1;
            bool ns_xattrs:1;
        } has, needs = {
            .id = projection->fsentry_mask & RBH_FP_ID,
            .parent_id = projection->fsentry_mask & RBH_FP_PARENT_ID,
            .name = projection->fsentry_mask & RBH_FP_NAME,
            .inode_xattrs = projection->fsentry_mask & RBH_FP_INODE_XATTRS,
            .ns_xattrs = projection->fsentry_mask & RBH_FP_NAMESPACE_XATTRS,
        };

next:
        fsentry = rbh_iter_next(convert->fsentries);
        if (fsentry == NULL)
            return -1;

        has.id = fsentry->mask & RBH_FP_ID;
        has.parent_id = fsentry->mask & RBH_FP_PARENT_ID;
        has.name = fsentry->mask & RBH_FP_NAME;
        has.inode_xattrs = fsentry->mask & RBH_FP_INODE_XATTRS
                        && fsentry->xattrs.inode.count;
        has.ns_xattrs = fsentry->mask & RBH_FP_NAMESPACE_XATTRS
                     && fsentry->xattrs.ns.count;

        if (!has.id)
            goto next;

        /* What kind of fsevent should this fsentry be converted into? */
        upsert = needs.id;
        inode_xattr = !upsert && needs.inode_xattrs && has.inode_xattrs;
        link = needs.parent_id && needs.name && has.parent_id && has.name;
        ns_xattr = !link && has.parent_id && has.name && needs.ns_xattrs
                && has.ns_xattrs && fsentry->xattrs.ns.count;
    } while (!(upsert || inode_xattr || link || ns_xattr));

    convert->fsentry = fsentry;
    convert->todo.upsert = upsert;
    convert->todo.inode_xattr = inode_xattr;
    convert->todo.link = link;
    convert->todo.ns_xattr = ns_xattr;
    return 0;
}

static struct rbh_statx *
statx_project(struct rbh_statx *dest, const struct rbh_statx *source,
              uint64_t mask)
{
    *dest = *source;
    dest->stx_mask &= mask;
    return dest;
}

static struct rbh_fsevent *
upsert_from_fsentry(struct rbh_fsevent *fsevent, struct rbh_statx *statx,
                    const struct rbh_fsentry *fsentry,
                    const struct rbh_filter_projection *projection)
{
    struct {
        bool xattrs:1;
        bool statx:1;
        bool symlink:1;
    } has = {
        .xattrs = fsentry->mask & RBH_FP_INODE_XATTRS,
        .statx = fsentry->mask & RBH_FP_STATX,
        .symlink = fsentry->mask & RBH_FP_SYMLINK,
    }, needs = {
        .xattrs = projection->fsentry_mask & RBH_FP_INODE_XATTRS,
        .statx = projection->fsentry_mask & RBH_FP_STATX,
        .symlink = projection->fsentry_mask & RBH_FP_SYMLINK,
    };

    fsevent->type = RBH_FET_UPSERT;
    assert(fsentry->mask & RBH_FP_ID);
    fsevent->id = fsentry->id;

    if (needs.xattrs & has.xattrs)
        fsevent->xattrs = fsentry->xattrs.inode;
    else
        fsevent->xattrs.count = 0;

    if (needs.statx & has.statx)
        fsevent->upsert.statx = statx_project(statx, fsentry->statx,
                                              projection->statx_mask);
    else
        fsevent->upsert.statx = NULL;

    if (needs.symlink & has.symlink)
        fsevent->upsert.symlink = fsentry->symlink;
    else
        fsevent->upsert.symlink = NULL;

    return fsevent;
}

static struct rbh_fsevent *
inode_xattr_from_fsentry(struct rbh_fsevent *fsevent,
                         const struct rbh_fsentry *fsentry)
{
    fsevent->type = RBH_FET_XATTR;
    assert(fsentry->mask & RBH_FP_ID);
    fsevent->id = fsentry->id;
    assert(fsentry->mask & RBH_FP_INODE_XATTRS);
    fsevent->xattrs = fsentry->xattrs.inode;

    fsevent->ns.parent_id = NULL;
    fsevent->ns.name = NULL;

    return fsevent;
}

static struct rbh_fsevent *
link_from_fsentry(struct rbh_fsevent *fsevent,
                  const struct rbh_fsentry *fsentry,
                  const struct rbh_filter_projection *projection)
{
    struct {
        bool xattrs:1;
    } has = {
        .xattrs = fsentry->mask & RBH_FP_NAMESPACE_XATTRS,
    }, needs = {
        .xattrs = projection->fsentry_mask & RBH_FP_NAMESPACE_XATTRS,
    };

    fsevent->type = RBH_FET_LINK;
    assert(fsentry->mask & RBH_FP_ID);
    fsevent->id = fsentry->id;
    assert(fsentry->mask & RBH_FP_PARENT_ID);
    fsevent->link.parent_id = &fsentry->parent_id;
    assert(fsentry->mask & RBH_FP_NAME);
    fsevent->link.name = fsentry->name;

    if (needs.xattrs & has.xattrs)
        fsevent->xattrs = fsentry->xattrs.ns;
    else
        fsevent->xattrs.count = 0;

    return fsevent;
}

static struct rbh_fsevent *
ns_xattr_from_fsentry(struct rbh_fsevent *fsevent,
                      const struct rbh_fsentry *fsentry)
{
    fsevent->type = RBH_FET_XATTR;
    assert(fsentry->mask & RBH_FP_ID);
    fsevent->id = fsentry->id;
    assert(fsentry->mask & RBH_FP_PARENT_ID);
    fsevent->link.parent_id = &fsentry->parent_id;
    assert(fsentry->mask & RBH_FP_NAME);
    fsevent->link.name = fsentry->name;
    assert(fsentry->mask & RBH_FP_NAMESPACE_XATTRS);
    fsevent->xattrs = fsentry->xattrs.ns;

    return fsevent;
}

static const void *
convert_iter_next(void *iterator)
{
    struct convert_iterator *convert = iterator;

    if (convert->todo.upsert) {
        convert->todo.upsert = false;
        return upsert_from_fsentry(&convert->fsevent, &convert->statx,
                                   convert->fsentry, convert->projection);
    }

    if (convert->todo.inode_xattr) {
        convert->todo.inode_xattr = false;
        return inode_xattr_from_fsentry(&convert->fsevent, convert->fsentry);
    }

    if (convert->todo.link) {
        convert->todo.link = false;
        return link_from_fsentry(&convert->fsevent, convert->fsentry,
                                 convert->projection);
    }

    if (convert->todo.ns_xattr) {
        convert->todo.ns_xattr = false;
        return ns_xattr_from_fsentry(&convert->fsevent, convert->fsentry);
    }

    if (_convert_iter_next(convert, convert->projection))
        return NULL;

    return convert_iter_next(iterator);
}

static void
convert_iter_destroy(void *iterator) {
    struct convert_iterator *convert = iterator;

    rbh_iter_destroy(convert->fsentries);
    free(convert);
}

static const struct rbh_iterator_operations CONVERT_ITER_OPS = {
    .next = convert_iter_next,
    .destroy = convert_iter_destroy,
};

static const struct rbh_iterator CONVERT_ITER = {
    .ops = &CONVERT_ITER_OPS,
};

static struct rbh_iterator *
iter_convert(struct rbh_iterator *fsentries,
             const struct rbh_filter_projection *projection)
{
    struct convert_iterator *convert;

    convert = malloc(sizeof(*convert));
    if (convert == NULL)
        return NULL;

    convert->iterator = CONVERT_ITER;
    convert->fsentries = fsentries;
    convert->projection = projection;
    convert->fsentry = NULL;
    convert->todo.upsert = convert->todo.link = false;
    convert->todo.inode_xattr = convert->todo.ns_xattr = false;

    return &convert->iterator;
}

static void
sync(const struct rbh_filter_projection *projection)
{
    const struct rbh_filter_options OPTIONS = {
        .projection = {
            .fsentry_mask = RBH_FP_ALL,
            .statx_mask = RBH_STATX_ALL,
        },
        .skip_error = skip_error,
    };
    struct rbh_mut_iterator *_fsentries;
    struct rbh_iterator *fsentries;
    struct rbh_iterator *fsevents;

    if (one) {
        struct rbh_fsentry *root;

        root = rbh_backend_root(from, &OPTIONS.projection);
        if (root == NULL)
            error(EXIT_FAILURE, errno, "rbh_backend_root");

        _fsentries = mut_iter_one(root);
        if (_fsentries == NULL)
            error(EXIT_FAILURE, errno, "rbh_mut_array_iterator");
    } else {
        /* "Dump" `from' */
        _fsentries = rbh_backend_filter(from, NULL, &OPTIONS);
        if (_fsentries == NULL)
            error(EXIT_FAILURE, errno, "rbh_backend_filter_fsentries");
    }

    fsentries = rbh_iter_constify(_fsentries);
    if (fsentries == NULL) {
        int save_errno = errno;

        rbh_mut_iter_destroy(_fsentries);
        error(EXIT_FAILURE, save_errno, "rbh_iter_constify");
    }

    /* Convert all this information into fsevents */
    fsevents = iter_convert(fsentries, projection);
    if (fsevents == NULL) {
        int save_errno = errno;

        rbh_iter_destroy(fsentries);
        error(EXIT_FAILURE, save_errno, "iter_convert");
    }

    /* XXX: the mongo backend tries to process all the fsevents at once in a
     *      single bulk operation, but a bulk operation is limited in size.
     *
     * Splitting `fsevents' into fixed-size sub-iterators solves this.
     */
    chunks = rbh_iter_chunkify(fsevents, RBH_ITER_CHUNK_SIZE);
    if (chunks == NULL) {
        int save_errno = errno;

        rbh_iter_destroy(fsevents);
        error(EXIT_FAILURE, save_errno, "rbh_mut_iter_chunkify");
    }

    /* Update `to' */
    do {
        struct rbh_iterator *chunk = rbh_mut_iter_next(chunks);
        int save_errno;
        ssize_t count;

        if (chunk == NULL) {
            if (errno == ENODATA || errno == RBH_BACKEND_ERROR)
                break;
            error(EXIT_FAILURE, errno, "while chunkifying SOURCE's entries");
        }

        count = rbh_backend_update(to, chunk, skip_error);
        save_errno = errno;
        rbh_iter_destroy(chunk);
        if (count < 0) {
            errno = save_errno;
            assert(errno != ENODATA);
            break;
        }
    } while (true);

    switch (errno) {
    case ENODATA:
        return;
    case RBH_BACKEND_ERROR:
        error(EXIT_FAILURE, 0, "unhandled error: %s", rbh_backend_error);
        __builtin_unreachable();
    default:
        error(EXIT_FAILURE, errno, "while iterating over SOURCE's entries");
    }
}

/*----------------------------------------------------------------------------*
 |                                list capabilities                           |
 *----------------------------------------------------------------------------*/
static void
list_capabilities(char *uri)
{
    const struct rbh_backend_plugin *plugin;
    struct rbh_backend *backend;

    plugin = rbh_backend_plugin_import(uri);
    if (plugin != NULL) {
        backend = rbh_backend_plugin_new(plugin, "none");
    } else {
        backend = rbh_backend_from_uri(uri);
        if (backend == NULL)
            error(EXIT_FAILURE, errno, "Unable to load backend %s", uri);
    }

    printf("*** Capabilities for %s backend ***\n", backend->name);
    printf("[%c] SOURCE backend\n", (backend->ops->filter ? 'x':' '));
    printf("[%c] DEST backend\n", (backend->ops->update ? 'x':' '));
    printf("[%c] BRANCH backend\n", (backend->ops->branch ? 'x':' '));
}

/*----------------------------------------------------------------------------*
 |                                    cli                                     |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                              usage()                               |
     *--------------------------------------------------------------------*/

static int
usage(void)
{
    const char *message =
        "usage: %s [-hon] [-f [+-]FIELD] SOURCE DEST\n"
        "\n"
        "Upsert SOURCE's entries into DEST\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE  a robinhood URI\n"
        "    DEST    a robinhood URI\n"
        "\n"
        "Optional arguments:\n"
        "    -f,--field [+-]FIELD  select, add or remove a FIELD to synchronize\n"
        "                          (can be specified multiple times)\n"
        "    -h,--help             show this message and exit\n"
        "    -o,--one              only consider the root of SOURCE\n"
        "    -n,--no-skip          do not skip errors when synchronizing backends,\n"
        "                          instead stop on the first error.\n"
        "\n"
        "Capability arguments:\n"
        "    -l,--list-capabilities URI|NAME\n"
        "                          print backend URI or NAME capabilities\n"
        "\n"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n"
        "Where:\n"
        "    BACKEND  is the name of a backend\n"
        "    FSNAME   is the name of a filesystem for BACKEND\n"
        "    PATH/ID  is the path/id of an fsentry managed by BACKEND:FSNAME\n"
        "             (ID must be enclosed in square brackets '[ID]' to distinguish it\n"
        "             from a path)\n"
        "\n"
        "FIELD can be any of the following:\n"
        "    [x] id          [x] parent-id   [x] name        [x] statx\n"
        "    [x] symlink     [x] ns-xattrs   [x] xattrs\n"
        "\n"
        "  Where 'statx' also supports the following subfields:\n"
        "    [x] blksize     [x] attributes  [x] nlink       [x] uid\n"
        "    [x] gid         [x] type        [x] mode        [x] ino\n"
        "    [x] size        [x] blocks      [x] atime.nsec  [x] atime.sec\n"
        "    [x] btime.nsec  [x] btime.sec   [x] ctime.nsec  [x] ctime.sec\n"
        "    [x] mtime.nsec  [x] mtime.sec   [x] rdev.major  [x] rdev.minor\n"
        "    [x] dev.major   [x] dev.minor   [ ] mount-id\n"
        "\n"
        "  [x] indicates the field is included by default\n"
        "  [ ] indicates the field is excluded by default\n";

    return printf(message, program_invocation_short_name);
}

static uint32_t
str2statx_field(const char *string_)
{
    const char *string = string_;

    switch (*string++) {
    case 'a': /* atime.nsec, atime.sec, attributes */
        if (*string++ != 't')
            break;

        switch (*string++) {
        case 'i': /* atime.nsec, atime.sec */
            if (strncmp(string, "me.", 3))
                break;
            string += 3;

            switch (*string++) {
            case 'n':
                if (strcmp(string, "sec"))
                    break;
                return RBH_STATX_ATIME_NSEC;
            case 's':
                if (strcmp(string, "ec"))
                    break;
                return RBH_STATX_ATIME_SEC;
            }
            break;
        case 't': /* attributes */
            if (strcmp(string, "ributes"))
                break;
            return RBH_STATX_ATTRIBUTES;
        }
        break;
    case 'b': /* blksize, blocks, btime.nsec, btime.sec */
        switch (*string++) {
        case 'l': /* blksize, blocks */
            switch (*string++) {
            case 'k': /* blksize */
                if (strcmp(string, "size"))
                    break;
                return RBH_STATX_BLKSIZE;
            case 'o': /* blocks */
                if (strcmp(string, "cks"))
                    break;
                return RBH_STATX_BLOCKS;
            }
            break;
        case 't': /* btime.nsec, btime.sec */
            if (strncmp(string, "ime.", 4))
                break;
            string += 4;

            switch (*string++) {
            case 'n':
                if (strcmp(string, "sec"))
                    break;
                return RBH_STATX_BTIME_NSEC;
            case 's':
                if (strcmp(string, "ec"))
                    break;
                return RBH_STATX_BTIME_SEC;
            }
            break;
        }
        break;
    case 'c': /* ctime.nsec, ctime.sec */
        if (strncmp(string, "time.", 5))
            break;
        string += 5;

        switch (*string++) {
        case 'n':
            if (strcmp(string, "sec"))
                break;
            return RBH_STATX_CTIME_NSEC;
        case 's':
            if (strcmp(string, "ec"))
                break;
            return RBH_STATX_CTIME_SEC;
        }
        break;
    case 'd': /* dev.major, dev.minor */
        if (strncmp(string, "ev.m", 4))
            break;
        string += 4;

        switch (*string++) {
        case 'a':
            if (strcmp(string, "jor"))
                break;
            return RBH_STATX_DEV_MAJOR;
        case 'i':
            if (strcmp(string, "nor"))
                break;
            return RBH_STATX_DEV_MINOR;
        }
        break;
    case 'g': /* gid */
        if (strcmp(string, "id"))
            break;
        return RBH_STATX_GID;
    case 'i': /* ino */
        if (strcmp(string, "no"))
            break;
        return RBH_STATX_INO;
    case 'm': /* mode, mtime.nsec, mtime.sec */
        switch (*string++) {
        case 'o': /* mode */
            if (strcmp(string, "de"))
                break;
            return RBH_STATX_MODE;
        case 't': /* mtime.nsec, mtime.sec */
            if (strncmp(string, "ime.", 4))
                break;
            string += 4;

            switch (*string++) {
            case 'n':
                if (strcmp(string, "sec"))
                    break;
                return RBH_STATX_MTIME_NSEC;
            case 's':
                if (strcmp(string, "ec"))
                    break;
                return RBH_STATX_MTIME_SEC;
            }
            break;
        }
        break;
    case 'n': /* nlink */
        if (strcmp(string, "link"))
            break;
        return RBH_STATX_NLINK;
    case 'r': /* rdev.major, rdev.minor */
        if (strncmp(string, "dev.m", 5))
            break;
        string += 5;

        switch (*string++) {
        case 'a':
            if (strcmp(string, "jor"))
                break;
            return RBH_STATX_DEV_MAJOR;
        case 'i':
            if (strcmp(string, "nor"))
                break;
            return RBH_STATX_DEV_MINOR;
        }
        break;
    case 's': /* size */
        if (strcmp(string, "ize"))
            break;
        return RBH_STATX_SIZE;
    case 't': /* type */
        if (strcmp(string, "ype"))
            break;
        return RBH_STATX_TYPE;
    case 'u': /* uid */
        if (strcmp(string, "id"))
            break;
        return RBH_STATX_UID;
    }

    error(EX_USAGE, 0, "unknown statx field: %s", string_);
    __builtin_unreachable();
}

static const struct rbh_filter_field *
str2field(const char *string_)
{
    static struct rbh_filter_field field;
    const char *string = string_;

    switch (*string++) {
    case 'i': /* id */
        if (strcmp(string, "d"))
            break;
        field.fsentry = RBH_FP_ID;
        return &field;
    case 'n': /* name, ns-xattrs */
        switch (*string++) {
        case 'a': /* name */
            if (strcmp(string, "me"))
                break;
            field.fsentry = RBH_FP_NAME;
            return &field;
        case 's': /* ns-xattrs */
            if (strncmp(string, "-xattrs", 7))
                break;
            field.fsentry = RBH_FP_NAMESPACE_XATTRS;
            string += 6;

            switch (*string++) {
            case '\0':
                field.xattr = NULL;
                return &field;
            case '.':
                field.xattr = string;
                return &field;
            }
            break;
        }
        break;
    case 'p': /* parent-id */
        if (strcmp(string, "arent-id"))
            break;
        field.fsentry = RBH_FP_PARENT_ID;
        return &field;
    case 's': /* statx, symlink */
        switch (*string++) {
        case 't':
            if (strncmp(string, "atx", 3))
                break;
            string += 3;

            field.fsentry = RBH_FP_STATX;
            switch (*string++) {
            case '\0':
                field.statx = RBH_STATX_ALL;
                return &field;
            case '.':
                field.statx = str2statx_field(string);
                return &field;
            }
            break;
        case 'y':
            if (strcmp(string, "mlink"))
                break;
            field.fsentry = RBH_FP_SYMLINK;
            return &field;
        }
        break;
    case 'x': /* xattrs */
        if (strncmp(string, "attrs", 5))
            break;
        string += 5;

        field.fsentry = RBH_FP_INODE_XATTRS;
        switch (*string++) {
        case '\0':
            field.xattr = NULL;
            return &field;
        case '.':
            field.xattr = string;
            return &field;
        }
        break;
    }

    error(EX_USAGE, 0, "unknown field: %s", string_);
    __builtin_unreachable();
}

static void
projection_add(struct rbh_filter_projection *projection,
               const struct rbh_filter_field *field)
{
    projection->fsentry_mask |= field->fsentry;

    switch (field->fsentry) {
    case RBH_FP_ID:
    case RBH_FP_PARENT_ID:
    case RBH_FP_NAME:
        break;
    case RBH_FP_STATX:
        projection->statx_mask |= field->statx;
        break;
    case RBH_FP_SYMLINK:
    case RBH_FP_NAMESPACE_XATTRS:
        // TODO: handle subfields
        break;
    case RBH_FP_INODE_XATTRS:
        // TODO: handle subfields
        break;
    }
}

static void
projection_remove(struct rbh_filter_projection *projection,
                  const struct rbh_filter_field *field)
{
    projection->fsentry_mask &= ~field->fsentry;

    switch (field->fsentry) {
    case RBH_FP_ID:
    case RBH_FP_PARENT_ID:
    case RBH_FP_NAME:
        break;
    case RBH_FP_STATX:
        projection->statx_mask &= ~field->statx;
        if (projection->statx_mask & RBH_STATX_ALL)
            projection->fsentry_mask |= RBH_FP_STATX;
        break;
    case RBH_FP_SYMLINK:
    case RBH_FP_NAMESPACE_XATTRS:
        // TODO: handle subfields
        break;
    case RBH_FP_INODE_XATTRS:
        // TODO: handle subfields
        break;
    }
}

static void
projection_set(struct rbh_filter_projection *projection,
               const struct rbh_filter_field *field)
{
    projection->fsentry_mask = field->fsentry;
    projection->statx_mask = 0;
    projection->xattrs.inode.count = 0;
    projection->xattrs.ns.count = 0;

    switch (field->fsentry) {
    case RBH_FP_ID:
    case RBH_FP_PARENT_ID:
    case RBH_FP_NAME:
        break;
    case RBH_FP_STATX:
        projection->statx_mask = field->statx;
        break;
    case RBH_FP_SYMLINK:
    case RBH_FP_NAMESPACE_XATTRS:
        // TODO: handle subfields
        break;
    case RBH_FP_INODE_XATTRS:
        // TODO: handle subfields
        break;
    }
}

int
main(int argc, char *argv[])
{
    const struct option LONG_OPTIONS[] = {
        {
            .name = "field",
            .has_arg = required_argument,
            .val = 'f',
        },
        {
            .name = "help",
            .val = 'h',
        },
        {
            .name = "list-capabilities",
            .val = 'l',
        },
        {
            .name = "one",
            .val = 'o',
        },
        {
            .name = "no-skip",
            .val = 'n',
        },
        {}
    };
    struct rbh_filter_projection projection = {
        .fsentry_mask = RBH_FP_ALL,
        .statx_mask = RBH_STATX_ALL & ~RBH_STATX_MNT_ID,
    };
    char c;

    /* Parse the command line */
    while ((c = getopt_long(argc, argv, "f:hl:on", LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'f':
            switch (optarg[0]) {
            case '+':
                projection_add(&projection, str2field(optarg + 1));
                break;
            case '-':
                projection_remove(&projection, str2field(optarg + 1));
                break;
            default:
                projection_set(&projection, str2field(optarg));
                break;
            }
            break;
        case 'h':
            usage();
            return 0;
        case 'l':
            list_capabilities(optarg);
            return EXIT_SUCCESS;
        case 'o':
            one = true;
            break;
        case 'n':
            skip_error = false;
            break;
        case '?':
        default:
            /* getopt_long() prints meaningful error messages itself */
            exit(EX_USAGE);
        }
    }

    argc -= optind;
    argv += optind;

    if (argc < 2)
        error(EX_USAGE, 0, "not enough arguments");
    if (argc > 2)
        error(EX_USAGE, 0, "unexpected argument: %s", argv[2]);

    /* Parse SOURCE */
    from = rbh_backend_from_uri(argv[0]);
    /* Parse DEST */
    to = rbh_backend_from_uri(argv[1]);

    sync(&projection);

    return EXIT_SUCCESS;
}

// vim: expandtab:ts=4:sw=4
