/* This file is part of rbh-sync
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
#include <robinhood/statx.h>
#include <robinhood/utils.h>

#ifndef RBH_ITER_CHUNK_SIZE
# define RBH_ITER_CHUNK_SIZE (1 << 12)
#endif

static struct rbh_backend *from, *to;

static void __attribute__((destructor))
destroy_from(void)
{
    if (from)
        rbh_backend_destroy(from);
}

static void __attribute__((destructor))
destroy_to(void)
{
    if (to)
        rbh_backend_destroy(to);
}

static struct rbh_mut_iterator *chunks;

static void __attribute__((destructor))
destroy_chunks(void)
{
    if (chunks)
        rbh_mut_iter_destroy(chunks);
}

static bool one = false;

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
    struct rbh_mut_iterator iterator;
    struct rbh_iterator *fsentries;

    const struct rbh_fsentry *fsentry;
    bool upsert;
    bool link;
};

/* Advance a convert_iterator to its next fsentry */
static int
_convert_iter_next(struct convert_iterator *convert)
{
    const struct rbh_fsentry *fsentry;
    bool upsert, link;

    do {
        fsentry = rbh_iter_next(convert->fsentries);
        if (fsentry == NULL)
            return -1;

        if (!(fsentry->mask & RBH_FP_ID)) {
            /* this should never happen */
            /* FIXME: log something about it */
            upsert = link = false;
            continue;
        }

        /* What kind of fsevent should this fsentry be converted to? */
        upsert = (fsentry->mask & RBH_FP_STATX)
              || (fsentry->mask & RBH_FP_SYMLINK);
        link = (fsentry->mask & RBH_FP_PARENT_ID)
            && (fsentry->mask & RBH_FP_NAME);
    } while (!upsert && !link);

    convert->fsentry = fsentry;
    convert->upsert = upsert;
    convert->link = link;
    return 0;
}

static void *
convert_iter_next(void *iterator)
{
    struct convert_iterator *convert = iterator;
    const struct rbh_fsentry *fsentry;
    struct rbh_fsevent *fsevent;

    /* Should the current fsentry generate any more fsevent? */
    if (!convert->upsert && !convert->link) {
        /* No => fetch the next one */
        if (_convert_iter_next(convert))
            return NULL;
        assert(convert->upsert || convert->link);
    }
    fsentry = convert->fsentry;

    if (convert->upsert) {
        struct {
            bool xattrs:1;
            bool statx:1;
            bool symlink:1;
        } has = {
            .xattrs = fsentry->mask & RBH_FP_INODE_XATTRS,
            .statx = fsentry->mask & RBH_FP_STATX,
            .symlink = fsentry->mask & RBH_FP_SYMLINK,
        };

        fsevent = rbh_fsevent_upsert_new(
                &fsentry->id,
                has.xattrs ? &fsentry->xattrs.inode : NULL,
                has.statx ? fsentry->statx : NULL,
                has.symlink ? fsentry->symlink : NULL
                );
        if (fsevent == NULL)
            return NULL;
        convert->upsert = false;
        return fsevent;
    }

    if (convert->link) {
        bool has_xattrs = fsentry->mask & RBH_FP_NAMESPACE_XATTRS;

        fsevent = rbh_fsevent_link_new(&fsentry->id,
                                       has_xattrs ? &fsentry->xattrs.ns : NULL,
                                       &fsentry->parent_id, fsentry->name);
        if (fsevent == NULL)
            return NULL;
        convert->link = false;
        return fsevent;
    }

    __builtin_unreachable();
}

static void
convert_iter_destroy(void *iterator) {
    struct convert_iterator *convert = iterator;

    rbh_iter_destroy(convert->fsentries);
    free(convert);
}

static const struct rbh_mut_iterator_operations CONVERT_ITER_OPS = {
    .next = convert_iter_next,
    .destroy = convert_iter_destroy,
};

static const struct rbh_mut_iterator CONVERT_ITER = {
    .ops = &CONVERT_ITER_OPS,
};

static struct rbh_iterator *
iter_convert(struct rbh_iterator *fsentries)
{
    struct convert_iterator *convert;
    struct rbh_iterator *constified;

    convert = malloc(sizeof(*convert));
    if (convert == NULL)
        return NULL;

    convert->iterator = CONVERT_ITER;
    convert->fsentries = fsentries;
    convert->fsentry = NULL;
    convert->upsert = convert->link = false;

    constified = rbh_iter_constify(&convert->iterator);
    if (constified == NULL) {
        int save_errno = errno;

        rbh_mut_iter_destroy(&convert->iterator);
        errno = save_errno;
    }

    return constified;
}

static void
sync(void)
{
    const struct rbh_filter_options OPTIONS = {
        .projection = {
            .fsentry_mask = RBH_FP_ALL,
            .statx_mask = RBH_STATX_ALL,
        },
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
    fsevents = iter_convert(fsentries);
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

        count = rbh_backend_update(to, chunk);
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
 |                                    cli                                     |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                              usage()                               |
     *--------------------------------------------------------------------*/

static int
usage(void)
{
    const char *message =
        "usage: %s [--one] SOURCE DEST\n"
        "\n"
        "Upsert SOURCE's entries into DEST\n"
        "\n"
        "Positional arguments:\n"
        "    SOURCE  a robinhood URI\n"
        "    DEST    a robinhood URI\n"
        "\n"
        "Optional arguments:\n"
        "    -h,--help  show this message and exit\n"
        "    -o,--one   only consider the root of SOURCE\n"
        "\n"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n"
        "Where:\n"
        "    BACKEND  is the name of a backend\n"
        "    FSNAME   is the name of a filesystem for BACKEND\n"
        "    PATH/ID  is the path/id of an fsentry managed by BACKEND:FSNAME\n"
        "             (ID must be enclosed in square brackets '[ID]' to distinguish it\n"
        "             from a path)\n";

    return printf(message, program_invocation_short_name);
}

int
main(int argc, char *argv[])
{
    const struct option LONG_OPTIONS[] = {
        {
            .name = "help",
            .val = 'h',
        },
        {
            .name = "one",
            .val = 'o',
        },
        {}
    };
    char c;

    /* Parse the command line */
    while ((c = getopt_long(argc, argv, "ho", LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'h':
            usage();
            return 0;
        case 'o':
            one = true;
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

    sync();

    return EXIT_SUCCESS;
}
