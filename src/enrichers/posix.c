/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include <robinhood.h>

#include "enricher.h"

struct enricher {
    struct rbh_iterator iterator;

    struct rbh_iterator *fsevents;
    int mount_fd;

    struct rbh_value_pair *pairs;
    size_t pair_count;

    struct rbh_fsevent fsevent;
    struct rbh_statx statx;
    char *symlink;
};

enum statx_field {
    SF_UNKNOWN,
    SF_TYPE,
    SF_MODE,
    SF_NLINK,
    SF_UID,
    SF_GID,
    SF_ATIME,
    SF_MTIME,
    SF_CTIME,
    SF_INO,
    SF_SIZE,
    SF_BLOCKS,
    SF_BTIME,
    SF_BLKSIZE,
    SF_ATTRIBUTES,
    SF_RDEV,
    SF_DEV,
};

static enum statx_field __attribute__((pure))
str2statx_field(const char *string)
{
    switch (*string++) {
    case 'a': /* atime, attributes */
        if (*string++ != 't')
            break;
        switch (*string++) {
        case 'i': /* atime */
            if (strcmp(string, "me"))
                break;
            return SF_ATIME;
        case 't': /* attributes */
            if (strcmp(string, "ributes"))
                break;
            return SF_ATTRIBUTES;
        }
        break;
    case 'b': /* blksize, blocks, btime */
        switch (*string++) {
        case 'l': /* blksize, blocks */
            switch (*string++) {
            case 'k': /* blksize */
                if (strcmp(string, "size"))
                    break;
                return SF_BLKSIZE;
            case 'o': /* blocks */
                if (strcmp(string, "cks"))
                    break;
                return SF_BLOCKS;
            }
            break;
        case 't': /* btime */
            if (strcmp(string, "ime"))
                break;
            return SF_BTIME;
        }
        break;
    case 'c': /* ctime */
        if (strcmp(string, "time"))
            break;
        return SF_CTIME;
    case 'd': /* dev */
        if (strcmp(string, "ev"))
            break;
        return SF_DEV;
    case 'g': /* gid */
        if (strcmp(string, "id"))
            break;
        return SF_GID;
    case 'i': /* ino */
        if (strcmp(string, "no"))
            break;
        return SF_INO;
    case 'm': /* mode, mtime */
        switch (*string++) {
        case 'o': /* mode */
            if (strcmp(string, "de"))
                break;
            return SF_MODE;
        case 't': /* mtime */
            if (strcmp(string, "ime"))
                break;
            return SF_MTIME;
        }
        break;
    case 'n': /* nlink */
        if (strcmp(string, "link"))
            break;
        return SF_NLINK;
    case 'r': /* rdev */
        if (strcmp(string, "dev"))
            break;
        return SF_RDEV;
    case 's': /* size */
        if (strcmp(string, "ize"))
            break;
        return SF_SIZE;
    case 't': /* type */
        if (strcmp(string, "ype"))
            break;
        return SF_TYPE;
    case 'u': /* uid */
        if (strcmp(string, "id"))
            break;
        return SF_UID;
    }
    return SF_UNKNOWN;
}

static const uint64_t statx_field2flag[] = {
    [SF_TYPE] = RBH_STATX_TYPE,
    [SF_MODE] = RBH_STATX_MODE,
    [SF_NLINK] = RBH_STATX_NLINK,
    [SF_UID] = RBH_STATX_UID,
    [SF_GID] = RBH_STATX_GID,
    [SF_ATIME] = RBH_STATX_ATIME,
    [SF_MTIME] = RBH_STATX_MTIME,
    [SF_CTIME] = RBH_STATX_CTIME,
    [SF_INO] = RBH_STATX_INO,
    [SF_SIZE] = RBH_STATX_SIZE,
    [SF_BLOCKS] = RBH_STATX_BLOCKS,
    [SF_BTIME] = RBH_STATX_BTIME,
    [SF_BLKSIZE] = RBH_STATX_BLKSIZE,
    [SF_ATTRIBUTES] = RBH_STATX_ATTRIBUTES,
    [SF_RDEV] = RBH_STATX_RDEV,
    [SF_DEV] = RBH_STATX_DEV,
};

enum statx_timestamp_field {
    STF_UNKNOWN,
    STF_SEC,
    STF_NSEC,
    _STF_COUNT,
};

static enum statx_timestamp_field __attribute__((pure))
str2statx_timestamp_field(const char *string)
{
    switch (*string++) {
    case 'n': /* nsec */
        if (strcmp(string, "sec"))
            break;
        return STF_NSEC;
    case 's': /* sec */
        if (strcmp(string, "ec"))
            break;
        return STF_SEC;
    }
    return STF_UNKNOWN;
}

static const uint64_t statx_timestamp_field2flag[][_STF_COUNT] = {
    [SF_ATIME] = {
        [STF_SEC] = RBH_STATX_ATIME_SEC,
        [STF_NSEC] = RBH_STATX_ATIME_NSEC,
    },
    [SF_BTIME] = {
        [STF_SEC] = RBH_STATX_BTIME_SEC,
        [STF_NSEC] = RBH_STATX_BTIME_NSEC,
    },
    [SF_CTIME] = {
        [STF_SEC] = RBH_STATX_CTIME_SEC,
        [STF_NSEC] = RBH_STATX_CTIME_NSEC,
    },
    [SF_MTIME] = {
        [STF_SEC] = RBH_STATX_MTIME_SEC,
        [STF_NSEC] = RBH_STATX_MTIME_NSEC,
    },
};

enum statx_device_field {
    SDF_UNKNOWN,
    SDF_MAJOR,
    SDF_MINOR,
    _SDF_COUNT,
};

static enum statx_device_field __attribute__((pure))
str2statx_device_field(const char *string)
{
    if (*string++ != 'm')
        return SDF_UNKNOWN;

    switch (*string++) {
    case 'a': /* major */
        if (strcmp(string, "jor"))
            break;
        return SDF_MAJOR;
    case 'i': /* minor */
        if (strcmp(string, "nor"))
            break;
        return SDF_MINOR;
    }
    return SDF_UNKNOWN;
}

static const uint64_t statx_device_field2flag[][_SDF_COUNT] = {
    [SF_RDEV] = {
        [SDF_MAJOR] = RBH_STATX_RDEV_MAJOR,
        [SDF_MINOR] = RBH_STATX_RDEV_MINOR,
    },
    [SF_DEV] = {
        [SDF_MAJOR] = RBH_STATX_DEV_MAJOR,
        [SDF_MINOR] = RBH_STATX_DEV_MINOR,
    },
};

static int
parse_statx_field(uint64_t *field, const struct rbh_value *value)
{
    const char *key = value->string;

    switch (value->type) {
    case RBH_VT_STRING:
        if (str2statx_field(key) == SF_UNKNOWN) {
            errno = ENOTSUP;
            return -1;
        }
        *field = statx_field2flag[str2statx_field(key)];
        return 0;
    case RBH_VT_MAP:
        if (value->map.count != 1) {
            errno = EINVAL;
            return -1;
        }
        key = value->map.pairs[0].key;
        value = value->map.pairs[0].value;
        if (value->type != RBH_VT_SEQUENCE) {
            errno = EINVAL;
            return -1;
        }

        *field = 0;
        switch (str2statx_field(key)) {
        case SF_UNKNOWN:
            errno = ENOTSUP;
            return -1;
        case SF_ATIME:
        case SF_BTIME:
        case SF_CTIME:
        case SF_MTIME:
            for (size_t i = 0; i < value->sequence.count; i++) {
                const struct rbh_value *subvalue = &value->sequence.values[i];
                const char *subkey;

                if (subvalue->type != RBH_VT_STRING) {
                    errno = EINVAL;
                    return -1;
                }

                subkey = subvalue->string;
                if (str2statx_timestamp_field(subkey) == STF_UNKNOWN) {
                    errno = EINVAL;
                    return -1;
                }
                *field |= statx_timestamp_field2flag[str2statx_field(key)]
                    [str2statx_timestamp_field(subkey)];
            }
            return 0;
        case SF_RDEV:
        case SF_DEV:
            for (size_t i = 0; i < value->sequence.count; i++) {
                const struct rbh_value *subvalue = &value->sequence.values[i];
                const char *subkey;

                if (subvalue->type != RBH_VT_STRING) {
                    errno = EINVAL;
                    return -1;
                }

                subkey = subvalue->string;
                if (str2statx_device_field(subkey) == SDF_UNKNOWN) {
                    errno = EINVAL;
                    return -1;
                }
                *field |= statx_device_field2flag[str2statx_field(key)]
                    [str2statx_device_field(subkey)];
            }
            return 0;
        default:
            errno = EINVAL;
            return -1;
        }
    default:
        errno = ENOTSUP;
        return -1;
    }
}

static int
parse_statx_mask(uint32_t *mask, const struct rbh_value *value)
{
    *mask = 0;

    if (value->type != RBH_VT_SEQUENCE && value->type != RBH_VT_UINT32) {
        errno = EINVAL;
        return -1;
    }

    if (value->type == RBH_VT_SEQUENCE) {
        for (size_t i = 0; i < value->sequence.count; i++) {
            uint64_t field;

            if (parse_statx_field(&field, &value->sequence.values[i]))
                return -1;
            *mask |= field;
        }
    } else {
        *mask |= value->uint32;
    }

    return 0;
}

enum partial_field {
    PF_UNKNOWN,
    PF_STATX,
    PF_XATTRS,
    PF_SYMLINK,
};

static enum partial_field __attribute__((pure))
str2partial_field(const char *string)
{
    switch (*string++) {
    case 's': /* statx, symlink */
        switch (*string++) {
        case 't': /* statx */
            if (strcmp(string, "atx")) {
                errno = ENOTSUP;
                return -1;
            }
            return PF_STATX;
        case 'y': /* symlink */
            if (strcmp(string, "mlink")) {
                errno = ENOTSUP;
                return -1;
            }
            return PF_SYMLINK;
        }
        break;
    case 'x': /* xattrs */
        if (strcmp(string, "attrs")) {
            errno = ENOTSUP;
            return -1;
        }
        return PF_XATTRS;
    }

    return PF_UNKNOWN;
}

static void
merge_statx(struct rbh_statx *original, const struct rbh_statx *override)
{
    original->stx_mask |= override->stx_mask;

    if (override->stx_mask & RBH_STATX_TYPE)
        original->stx_mode |= override->stx_mode & S_IFMT;

    if (override->stx_mask & RBH_STATX_MODE)
        original->stx_mode |= override->stx_mode & ~S_IFMT;

    if (override->stx_mask & RBH_STATX_NLINK)
        original->stx_nlink = override->stx_nlink;

    if (override->stx_mask & RBH_STATX_UID)
        original->stx_uid = override->stx_uid;

    if (override->stx_mask & RBH_STATX_GID)
        original->stx_gid = override->stx_gid;

    if (override->stx_mask & RBH_STATX_ATIME_SEC)
        original->stx_atime.tv_sec = override->stx_atime.tv_sec;

    if (override->stx_mask & RBH_STATX_CTIME_SEC)
        original->stx_ctime.tv_sec = override->stx_ctime.tv_sec;

    if (override->stx_mask & RBH_STATX_MTIME_SEC)
        original->stx_mtime.tv_sec = override->stx_mtime.tv_sec;

    if (override->stx_mask & RBH_STATX_INO)
        original->stx_ino = override->stx_ino;

    if (override->stx_mask & RBH_STATX_SIZE)
        original->stx_size = override->stx_size;

    if (override->stx_mask & RBH_STATX_BLOCKS)
        original->stx_blocks = override->stx_blocks;

    if (override->stx_mask & RBH_STATX_BTIME_SEC)
        original->stx_btime.tv_sec = override->stx_btime.tv_sec;

    if (override->stx_mask & RBH_STATX_MNT_ID)
        original->stx_mnt_id = override->stx_mnt_id;

    if (override->stx_mask & RBH_STATX_BLKSIZE)
        original->stx_blksize = override->stx_blksize;

    if (override->stx_mask & RBH_STATX_ATTRIBUTES) {
        original->stx_attributes_mask = override->stx_attributes_mask;
        original->stx_attributes = override->stx_attributes;
    }

    if (override->stx_mask & RBH_STATX_ATIME_NSEC)
        original->stx_atime.tv_nsec = override->stx_atime.tv_nsec;

    if (override->stx_mask & RBH_STATX_BTIME_NSEC)
        original->stx_btime.tv_nsec = override->stx_btime.tv_nsec;

    if (override->stx_mask & RBH_STATX_CTIME_NSEC)
        original->stx_ctime.tv_nsec = override->stx_ctime.tv_nsec;

    if (override->stx_mask & RBH_STATX_MTIME_NSEC)
        original->stx_mtime.tv_nsec = override->stx_mtime.tv_nsec;

    if (override->stx_mask & RBH_STATX_RDEV_MAJOR)
        original->stx_rdev_major = override->stx_rdev_major;

    if (override->stx_mask & RBH_STATX_RDEV_MINOR)
        original->stx_rdev_minor = override->stx_rdev_minor;

    if (override->stx_mask & RBH_STATX_DEV_MAJOR)
        original->stx_dev_major = override->stx_dev_major;

    if (override->stx_mask & RBH_STATX_DEV_MINOR)
        original->stx_dev_minor = override->stx_dev_minor;
}

static int
open_by_id(int mount_fd, const struct rbh_id *id, int flags)
{
    struct file_handle *handle;
    int save_errno;
    int fd;

    handle = rbh_file_handle_from_id(id);
    if (handle == NULL)
        return -1;

    fd = open_by_handle_at(mount_fd, handle, flags);
    save_errno = errno;
    free(handle);
    errno = save_errno;
    return fd;
}

static int
enrich_statx(struct rbh_statx *dest, const struct rbh_id *id, int mount_fd,
             uint32_t mask, const struct rbh_statx *original)
{
    static const int STATX_FLAGS = AT_STATX_FORCE_SYNC | AT_EMPTY_PATH
                                 | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW;
    struct rbh_statx statxbuf;
    int save_errno;
    int rc;
    int fd;

    fd = open_by_id(mount_fd, id, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_PATH);
    if (fd == -1)
        return -1;

    /* FIXME: We should really use AT_RBH_STATX_FORCE_SYNC here */
    rc = rbh_statx(fd, "", STATX_FLAGS, mask, &statxbuf);
    save_errno = errno;
    /* Ignore errors on close */
    close(fd);
    if (rc == -1) {
        errno = save_errno;
        return -1;
    }

    if (original)
        *dest = *original;
    else
        dest->stx_mask = 0;
    merge_statx(dest, &statxbuf);
    return 0;
}

/* The Linux VFS does not allow values of more than 64KiB */
static const size_t XATTR_VALUE_MAX_VFS_SIZE = 1 << 16;

#define MIN_XATTR_VALUES_ALLOC 32
static __thread struct rbh_sstack *xattrs_values;

static void __attribute__((destructor))
exit_xattrs_values(void)
{
    if (xattrs_values)
        rbh_sstack_destroy(xattrs_values);
}

static int
enrich_xattrs(const struct rbh_value *xattrs_to_enrich,
              struct rbh_value_pair **pairs, size_t *pair_count,
              struct rbh_fsevent *enriched,
              const struct rbh_id *id, int mount_fd)
{
    char buffer[XATTR_VALUE_MAX_VFS_SIZE];
    const struct rbh_value *xattrs_seq;
    struct rbh_value *value;
    size_t xattrs_count;
    ssize_t length;
    int save_errno;
    int rc = 0;
    int fd;

    if (xattrs_to_enrich->type != RBH_VT_SEQUENCE) {
        errno = EINVAL;
        return -1;
    }

    if (xattrs_values == NULL) {
        xattrs_values = rbh_sstack_new(MIN_XATTR_VALUES_ALLOC *
                                       sizeof(struct rbh_value *));
        if (xattrs_values == NULL)
            return -1;
    }

    xattrs_seq = xattrs_to_enrich->sequence.values;
    xattrs_count = xattrs_to_enrich->sequence.count;

    fd = open_by_id(mount_fd, id, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0 && errno == ELOOP)
        /* If the file to open is a symlink, reopen it with O_PATH set */
        fd = open_by_id(mount_fd, id, O_RDONLY | O_CLOEXEC | O_NOFOLLOW |
                        O_PATH);

    if (fd < 0) {
        rc = -1;
        goto err;
    }

    if (enriched->xattrs.count + xattrs_count >= *pair_count) {
        void *tmp;

        tmp = reallocarray(*pairs, *pair_count + xattrs_count, sizeof(**pairs));
        if (tmp == NULL) {
            rc = -1;
            goto err;
        }

        *pairs = tmp;
        *pair_count = *pair_count + xattrs_count;
    }

    for (size_t i = 0; i < xattrs_count; i++) {
        const char *key = xattrs_seq[i].string;

        length = fgetxattr(fd, key, buffer, sizeof(buffer));
        if (length == -1) {
            value = NULL;
        } else {
            value = rbh_sstack_push(xattrs_values, NULL, sizeof(*value));
            if (value == NULL) {
                rc = -1;
                goto err;
            }

            value->type = RBH_VT_BINARY;
            value->binary.data = rbh_sstack_push(xattrs_values, buffer, length);
            if (value->binary.data == NULL) {
                rc = -1;
                goto err;
            }

            value->binary.size = length;
        }

        assert(length <= (ssize_t) sizeof(buffer));

        (*pairs)[enriched->xattrs.count].key = xattrs_seq[i].string;
        (*pairs)[enriched->xattrs.count].value = value;
        enriched->xattrs.count++;
    }

err:
    save_errno = errno;
    /* Ignore errors on close */
    close(fd);
    errno = save_errno;
    return rc;
}

/* The Linux VFS doesn't allow for symlinks of more than 64KiB */
#define SYMLINK_MAX_SIZE (1 << 16)

static int
enrich_symlink(char symlink[SYMLINK_MAX_SIZE], const struct rbh_id *id,
               int mount_fd)
{
    int save_errno;
    ssize_t rc;
    int fd;

    fd = open_by_id(mount_fd, id, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_PATH);
    if (fd == -1)
        return -1;

    rc = readlinkat(fd, "", symlink, SYMLINK_MAX_SIZE);
    save_errno = errno;
    /* Ignore errors on close */
    close(fd);
    errno = save_errno;
    return rc == -1 ? -1 : 0;
}

static int
_enrich(const struct rbh_value_pair *partial, struct rbh_value_pair **pairs,
        size_t *pair_count, struct rbh_fsevent *enriched,
        const struct rbh_fsevent *original, int mount_fd,
        struct rbh_statx *statxbuf, char symlink[SYMLINK_MAX_SIZE])
{
    uint32_t statx_mask;

    switch (str2partial_field(partial->key)) {
    case PF_UNKNOWN:
        errno = ENOTSUP;
        return -1;
    case PF_STATX:
        if (original->type != RBH_FET_UPSERT) {
            errno = EINVAL;
            return -1;
        }

        if (parse_statx_mask(&statx_mask, partial->value))
            return -1;
        if (enrich_statx(statxbuf, &original->id, mount_fd, statx_mask,
                         original->upsert.statx))
            return -1;

        enriched->upsert.statx = statxbuf;
        break;
    case PF_XATTRS:
        if (original->type != RBH_FET_XATTR && original->type != RBH_FET_LINK) {
            errno = EINVAL;
            return -1;
        }

        if (enrich_xattrs(partial->value, pairs, pair_count, enriched,
                          &original->id, mount_fd))
            return -1;

        break;
    case PF_SYMLINK:
        if (original->type != RBH_FET_UPSERT) {
            errno = EINVAL;
            return -1;
        }

        if (enrich_symlink(symlink, &original->id, mount_fd))
            return -1;

        enriched->upsert.symlink = symlink;
        break;
    }
    return 0;
}

static int
enrich(struct enricher *enricher, const struct rbh_fsevent *original)
{
    struct rbh_fsevent *enriched = &enricher->fsevent;
    struct rbh_value_pair *pairs = enricher->pairs;
    size_t pair_count = enricher->pair_count;

    *enriched = *original;
    enriched->xattrs.count = 0;

    for (size_t i = 0; i < original->xattrs.count; i++) {
        const struct rbh_value_pair *pair = &original->xattrs.pairs[i];
        const struct rbh_value_map *partials;

        if (strcmp(pair->key, "rbh-fsevents")) {
            /* XXX: this could be made more efficient by copying ranges of
             *      xattrs (ie. "pairs") after each occurence of "rbh-fsevents".
             */
            if (enriched->xattrs.count + 1 >= pair_count) {
                void *tmp;

                tmp = reallocarray(pairs, pair_count << 1, sizeof(*pairs));
                if (tmp == NULL)
                    return -1;
                enricher->pairs = pairs = tmp;
                enricher->pair_count = pair_count = pair_count << 1;
            }
            pairs[enriched->xattrs.count++] = original->xattrs.pairs[i];
            continue;
        }

        if (pair->value == NULL || pair->value->type != RBH_VT_MAP) {
            errno = EINVAL;
            return -1;
        }
        partials = &pair->value->map;

        for (size_t i = 0; i < partials->count; i++) {
            if (_enrich(&partials->pairs[i], &pairs, &pair_count, enriched,
                        original, enricher->mount_fd, &enricher->statx,
                        enricher->symlink))
                return -1;
        }

    }
    enriched->xattrs.pairs = pairs;

    return 0;
}

static const void *
enricher_iter_next(void *iterator)
{
    struct enricher *enricher = iterator;
    const void *fsevent;

    fsevent = rbh_iter_next(enricher->fsevents);
    if (fsevent == NULL)
        return NULL;

    if (enrich(enricher, fsevent))
        return NULL;

    return &enricher->fsevent;
}

static void
enricher_iter_destroy(void *iterator)
{
    struct enricher *enricher = iterator;

    rbh_iter_destroy(enricher->fsevents);
    free(enricher->symlink);
    free(enricher->pairs);
    free(enricher);
}

static const struct rbh_iterator_operations ENRICHER_ITER_OPS = {
    .next = enricher_iter_next,
    .destroy = enricher_iter_destroy,
};

static const struct rbh_iterator ENRICHER_ITERATOR = {
    .ops = &ENRICHER_ITER_OPS,
};

#define INITIAL_PAIR_COUNT (1 << 7)

struct rbh_iterator *
iter_enrich(struct rbh_iterator *fsevents, int mount_fd)
{
    struct rbh_value_pair *pairs;
    struct enricher *enricher;
    char *symlink;

    symlink = malloc(SYMLINK_MAX_SIZE);
    if (symlink == NULL)
        return NULL;

    pairs = reallocarray(NULL, INITIAL_PAIR_COUNT, sizeof(*pairs));
    if (pairs == NULL) {
        int save_errno = errno;

        free(symlink);
        errno = save_errno;
        return NULL;
    }

    enricher = malloc(sizeof(*enricher));
    if (enricher == NULL) {
        int save_errno = errno;

        free(pairs);
        free(symlink);
        errno = save_errno;
        return NULL;
    }

    enricher->iterator = ENRICHER_ITERATOR;
    enricher->fsevents = fsevents;
    enricher->mount_fd = mount_fd;
    enricher->pairs = pairs;
    enricher->pair_count = INITIAL_PAIR_COUNT;
    enricher->symlink = symlink;

    return &enricher->iterator;
}

struct no_partial_iterator {
    struct rbh_iterator iterator;
    struct rbh_iterator *fsevents;
};

static const void *
no_partial_iter_next(void *iterator)
{
    struct no_partial_iterator *no_partial = iterator;
    const struct rbh_fsevent *fsevent;

    fsevent = rbh_iter_next(no_partial->fsevents);
    if (fsevent == NULL)
        return fsevent;

    for (size_t i = 0; i < fsevent->xattrs.count; i++) {
        const struct rbh_value_pair *pair = &fsevent->xattrs.pairs[i];

        if (strcmp(pair->key, "rbh-fsevents") == 0) {
            error(0, 0, "unexpected partial fsevent detected");
            /* The reasoning being that partial fsevents are internal to
             * rbh-fsevents and should not leak to backends, where others might
             * be tempted to interpret them.
             */
            errno = EINVAL;
            return NULL;
        }
    }

    return fsevent;
}

static void
no_partial_iter_destroy(void *iterator)
{
    struct no_partial_iterator *no_partial = iterator;

    rbh_iter_destroy(no_partial->fsevents);
    free(no_partial);
}

static const struct rbh_iterator_operations NO_PARTIAL_ITER_OPS = {
    .next = no_partial_iter_next,
    .destroy = no_partial_iter_destroy,
};

static const struct rbh_iterator NO_PARTIAL_ITERATOR = {
    .ops = &NO_PARTIAL_ITER_OPS,
};

struct rbh_iterator *
iter_no_partial(struct rbh_iterator *fsevents)
{
    struct no_partial_iterator *no_partial;

    no_partial = malloc(sizeof(*no_partial));
    if (no_partial == NULL)
        return NULL;

    no_partial->iterator = NO_PARTIAL_ITERATOR;
    no_partial->fsevents = fsevents;
    return &no_partial->iterator;
}


static struct rbh_iterator *
posix_enrich_iter_builder_build_iter(void *_builder,
                                     struct rbh_iterator *fsevents)
{
    struct enrich_iter_builder *builder = _builder;

    return iter_enrich(fsevents, builder->mount_fd);
}

static void
posix_enrich_iter_builder_destroy(void *_builder)
{
    struct enrich_iter_builder *builder = _builder;

    rbh_backend_destroy(builder->backend);
    close(builder->mount_fd);
    free(builder);
}

static const struct enrich_iter_builder_operations
POSIX_ENRICH_ITER_BUILDER_OPS = {
    .build_iter = posix_enrich_iter_builder_build_iter,
    .destroy = posix_enrich_iter_builder_destroy,
};

static const struct enrich_iter_builder POSIX_ENRICH_ITER_BUILDER = {
    .name = "posix",
    .ops = &POSIX_ENRICH_ITER_BUILDER_OPS,
};

struct enrich_iter_builder *
enrich_iter_builder_from_backend(struct rbh_backend *backend,
                                 const char *mount_path)
{
    struct enrich_iter_builder *builder;

    builder = malloc(sizeof(*builder));
    if (builder == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    *builder = POSIX_ENRICH_ITER_BUILDER;
    builder->backend = backend;

    builder->mount_fd = open(mount_path, O_RDONLY | O_CLOEXEC);
    if (builder->mount_fd == -1)
        error(EXIT_FAILURE, errno, "open: %s", mount_path);

    return builder;
}
