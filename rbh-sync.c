/* This file is part of rbh-sync
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <dlfcn.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include <sys/stat.h>

#include <robinhood.h>
#ifndef HAVE_STATX
# include <robinhood/statx.h>
#endif
#include <robinhood/utils.h>

#ifndef RBH_ITER_CHUNK_SIZE
# define RBH_ITER_CHUNK_SIZE (1 << 12)
#endif

static struct rbh_backend *from, *to;

static void
destroy_from(void)
{
    rbh_backend_destroy(from);
}

static void
destroy_to(void)
{
    rbh_backend_destroy(to);
}

static struct rbh_mut_iterator *chunks;

static void
destroy_chunks(void)
{
    rbh_mut_iter_destroy(chunks);
}

static void
_atexit(void (*function)(void))
{
    if (atexit(function))
        error(EXIT_FAILURE, 0, "cannot set atexit function");
}

/* A convert_iterator converts fsentries into fsevents.
 *
 * For each fsentry, it yields up to two fsevents (depending on the information
 * available in the fsentry): one RBH_FET_UPSERT, to create the inode in the
 * backend; and one RBH_FET_LINK to "link" the inode in the namespace.
 */
struct convert_iterator {
    struct rbh_mut_iterator iterator;
    struct rbh_mut_iterator *fsentries;
    struct rbh_fsentry *fsentry;

    bool upsert;
    bool link;
};

/* Advance a convert_iterator to its next fsentry */
static int
_convert_iter_next(struct convert_iterator *convert)
{
    struct rbh_fsentry *fsentry = convert->fsentry;
    bool upsert, link;

    convert->fsentry = NULL;
    do {
        free(fsentry);

        fsentry = rbh_mut_iter_next(convert->fsentries);
        if (fsentry == NULL)
            return -1;

        if (!(fsentry->mask & RBH_FP_ID))
            /* this should never happen */
            /* FIXME: log something about it */
            continue;

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
    struct rbh_fsentry *fsentry;
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

    rbh_mut_iter_destroy(convert->fsentries);
    free(convert->fsentry);
    free(convert);
}

static const struct rbh_mut_iterator_operations CONVERT_ITER_OPS = {
    .next = convert_iter_next,
    .destroy = convert_iter_destroy,
};

static const struct rbh_mut_iterator CONVERT_ITER = {
    .ops = &CONVERT_ITER_OPS,
};

static struct rbh_mut_iterator *
convert_iter_new(struct rbh_mut_iterator *fsentries)
{
    struct convert_iterator *convert;

    convert = malloc(sizeof(*convert));
    if (convert == NULL)
        return NULL;

    convert->iterator = CONVERT_ITER;
    convert->fsentries = fsentries;
    convert->fsentry = NULL;
    convert->upsert = convert->link = false;

    return &convert->iterator;
}

static int
upsert_fsentries(struct rbh_backend *backend,
                 struct rbh_mut_iterator *fsevents)
{
    struct rbh_mut_iterator *fsevents_tee[2];
    int save_errno;
    ssize_t rc;

    if (rbh_mut_iter_tee(fsevents, fsevents_tee))
        return -1;

    rc = rbh_backend_update(backend, (struct rbh_iterator *)fsevents_tee[0]);
    save_errno = errno;
    rbh_mut_iter_destroy(fsevents_tee[0]);

    /* Free all the fsevents */
    do {
        struct rbh_fsevent *fsevent = rbh_mut_iter_next(fsevents_tee[1]);

        if (fsevent == NULL)
            break;
        free(fsevent);
    } while (true);

    /* The previous loop should not exit without setting errno */
    assert(errno);

    /* If rbh_backend_update() failed, the value of errno at the time is what
     * we want to restore on return.
     */
    if (rc >= 0)
        save_errno = errno;

    rbh_mut_iter_destroy(fsevents_tee[1]);
    errno = save_errno;
    return rc >= 0 && errno == ENODATA ? 0 : -1;
}

static int
usage(FILE *output)
{
    return fprintf(output, "usage: %s [--one] SOURCE DEST\n"
"Positional arguments:\n"
"    SOURCE    a robinhood URI\n"
"    DEST      a robinhood URI\n"
"\n"
"Optional arguments:\n"
"    -o,--one  only synchronize one entry of SOURCE, its root\n"
"\n"
"A robinhood URI is built as follows:\n"
"    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n"
"where:\n"
"    BACKEND    is the name of a backend\n"
"    FSNAME     is the name of a filesystem for BACKEND\n"
"    PATH/ID    is the path/id of an fsentry managed by BACKEND:FSNAME (ID must\n"
"               be enclosed in square brackets '[ID]' to distinguish it from a\n"
"               path)\n", program_invocation_short_name);
}

/* CLI parser */

enum command_line_option {
    CLO_UNKNOWN,
    CLO_ONE,
};

static enum command_line_option
letter2option(char c)
{
    switch (c) {
    case 'o':
        return CLO_ONE;
    }
    return CLO_UNKNOWN;
}

static enum command_line_option
str2option(const char *string)
{
    switch (*string++) {
    case 'o':
        if (strcmp(string, "ne"))
            break;
        return CLO_ONE;
    }
    return CLO_UNKNOWN;
}

enum command_line_token {
    CLT_URI,
    CLT_DOUBLE_DASH,
    CLT_SHORT_OPTION,
    CLT_LONG_OPTION,
};

static bool double_dash = false;

static enum command_line_token
str2command_line_token(const char *string)
{
    if (double_dash)
        return CLT_URI;

    switch (*string++) {
    case '-':
        if (*string++ == '-') {
            if (*string++ == '\0')
                return CLT_DOUBLE_DASH;
            else
                return CLT_LONG_OPTION;
        }
        return CLT_SHORT_OPTION;
    default:
        return CLT_URI;
    }
}

static struct {
    bool one:1;
} options;

static void
set_option(enum command_line_option option)
{
    switch (option) {
    case CLO_ONE:
        options.one = true;
        break;
    default:
        error(EX_SOFTWARE, 0, "unreachable code");
    }
}

static void
parse(int argc, char *argv[])
{
    enum command_line_option option;

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];

        switch (str2command_line_token(arg)) {
        case CLT_DOUBLE_DASH:
            double_dash = true;
            break;
        case CLT_URI:
            if (from == NULL) {
                from = rbh_backend_from_uri(arg);
                _atexit(destroy_from);
            } else if (to == NULL) {
                to = rbh_backend_from_uri(arg);
                _atexit(destroy_to);
            } else {
                usage(stderr);
                error(EX_USAGE, 0, "unexpected argument: %s", arg);
            }
            break;
        case CLT_SHORT_OPTION:
            /* Discard the leading '-' */
            arg++;

            /* Process each letter separately */
            do {
                option = letter2option(*arg);
                if (option == CLO_UNKNOWN) {
                    usage(stderr);
                    error(EX_USAGE, 0, "unknown option: -%c", *arg);
                }

                set_option(option);
            } while (*++arg != '\0');

            break;
        case CLT_LONG_OPTION:
            /* Discard the leading "--" */
            option = str2option(arg + 2);
            if (option == CLO_UNKNOWN) {
                usage(stderr);
                error(EX_USAGE, 0, "unknown option: %s", arg);
            }

            set_option(option);
            break;
        }
    }

    if (from == NULL) {
        usage(stderr);
        error(EX_USAGE, 0, "missing SOURCE argument");
    }
    if (to == NULL) {
        usage(stderr);
        error(EX_USAGE, 0, "missing DEST argument");
    }
}

int
main(int argc, char *argv[])
{
    const struct rbh_filter_options OPTIONS = {
        .projection = {
            .fsentry_mask = RBH_FP_ALL,
            .statx_mask = STATX_ALL,
        },
    };
    struct rbh_mut_iterator *fsentries;
    struct rbh_mut_iterator *fsevents;

    parse(argc, argv);

    if (options.one) {
        struct rbh_fsentry *root;

        root = rbh_backend_root(from, &OPTIONS.projection);
        if (root == NULL)
            error(EXIT_FAILURE, errno, "rbh_backend_root");

        fsentries = rbh_mut_iter_array(root, sizeof(*root), 1);
        if (fsentries == NULL)
            error(EXIT_FAILURE, errno, "rbh_mut_array_iterator");
    } else {
        /* "Dump" `from' */
        fsentries = rbh_backend_filter(from, NULL, &OPTIONS);
        if (fsentries == NULL)
            error(EXIT_FAILURE, errno, "rbh_backend_filter_fsentries");
    }

    /* Convert all this information into fsevents */
    fsevents = convert_iter_new(fsentries);
    if (fsevents == NULL) {
        int save_errno = errno;
        rbh_mut_iter_destroy(fsentries);
        error(EXIT_FAILURE, save_errno, "convert_iter_new");
    }

    /* upsert_fsentries() cannot free fsevents as they are consumed. It has to
     * wait until the rbh_backend_update() returns. As there is no limit to the
     * number of elements `fsevents' may yield, this could lead to memory
     * exhaustion.
     *
     * Splitting `fsevents' into fixed-size sub-iterators solves this.
     */
    chunks = rbh_mut_iter_chunkify(fsevents, RBH_ITER_CHUNK_SIZE);
    if (chunks == NULL) {
        int save_errno = errno;
        rbh_mut_iter_destroy(fsevents);
        error(EXIT_FAILURE, save_errno, "rbh_mut_iter_chunkify");
    }
    _atexit(destroy_chunks);

    /* Update `to' */
    do {
        struct rbh_mut_iterator *chunk = rbh_mut_iter_next(chunks);

        if (chunk == NULL) {
            if (errno == ENODATA)
                break;
            error(EXIT_FAILURE, errno, "while chunkifying SOURCE's entries");
        }

        if (upsert_fsentries(to, chunk)) {
            assert(errno != ENODATA);
            break;
        }
    } while (true);

    switch (errno) {
    case ENODATA:
        return EXIT_SUCCESS;
    case RBH_BACKEND_ERROR:
        error(EXIT_FAILURE, 0, "unhandled error: %s\n", rbh_backend_error);
        __builtin_unreachable();
    default:
        error(EXIT_FAILURE, errno, "while iterating over SOURCE's entries");
        __builtin_unreachable();
    }
}
