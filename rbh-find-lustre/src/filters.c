/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <error.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include <lustre/lustreapi.h>

#include <robinhood/statx.h>

#include <robinhood/backend.h>
#include <robinhood/utils.h>
#include <rbh-find/filters.h>

#include "filters.h"

static const struct rbh_filter_field predicate2filter_field[] = {
    [LPRED_COMP_END - LPRED_MIN]       = {.fsentry = RBH_FP_INODE_XATTRS,
                                          .xattr = "end"},
    [LPRED_COMP_START - LPRED_MIN]     = {.fsentry = RBH_FP_INODE_XATTRS,
                                          .xattr = "begin"},
    [LPRED_FID - LPRED_MIN]            = {.fsentry = RBH_FP_INODE_XATTRS,
                                          .xattr = "fid"},
    [LPRED_HSM_STATE - LPRED_MIN]      = {.fsentry = RBH_FP_INODE_XATTRS,
                                          .xattr = "hsm_state"},
    [LPRED_LAYOUT_PATTERN - LPRED_MIN] = {.fsentry = RBH_FP_INODE_XATTRS,
                                          .xattr = "pattern"},
    [LPRED_MDT_COUNT - LPRED_MIN]      = {.fsentry = RBH_FP_INODE_XATTRS,
                                          .xattr = "mdt_count"},
    [LPRED_MDT_INDEX - LPRED_MIN]      = {.fsentry = RBH_FP_INODE_XATTRS,
                                          .xattr = "mdt_index"},
    [LPRED_OST_INDEX - LPRED_MIN]      = {.fsentry = RBH_FP_INODE_XATTRS,
                                          .xattr = "ost"},
    [LPRED_POOL - LPRED_MIN]           = {.fsentry = RBH_FP_INODE_XATTRS,
                                          .xattr = "pool"},
    [LPRED_STRIPE_COUNT - LPRED_MIN]   = {.fsentry = RBH_FP_INODE_XATTRS,
                                          .xattr = "stripe_count"},
    [LPRED_STRIPE_SIZE - LPRED_MIN]    = {.fsentry = RBH_FP_INODE_XATTRS,
                                          .xattr = "stripe_size"},
    [LPRED_EXPIRED - LPRED_MIN]        = {.fsentry = RBH_FP_INODE_XATTRS,
                                          .xattr = "trusted.expiration_date"},
};

static inline const struct rbh_filter_field *
get_filter_field(enum lustre_predicate predicate)
{
    return &predicate2filter_field[predicate - LPRED_MIN];
}

static enum hsm_states
str2hsm_states(const char *hsm_state)
{
    switch (hsm_state[0]) {
    case 'a':
        if (strcmp(&hsm_state[1], "rchived"))
            error(EX_USAGE, 0, "invalid argument to -hsm-state");
        return HS_ARCHIVED;
    case 'd':
        if (strcmp(&hsm_state[1], "irty"))
            error(EX_USAGE, 0, "invalid argument to -hsm-state");
        return HS_DIRTY;
    case 'e':
        if (strcmp(&hsm_state[1], "xists"))
            error(EX_USAGE, 0, "invalid argument to -hsm-state");
        return HS_EXISTS;
    case 'l':
        if (strcmp(&hsm_state[1], "ost"))
            error(EX_USAGE, 0, "invalid argument to -hsm-state");
        return HS_LOST;
    case 'n':
        if (hsm_state[1] != 'o')
            error(EX_USAGE, 0, "invalid argument to -hsm-state");
        switch (hsm_state[2]) {
        case 'a':
            if (strcmp(&hsm_state[3], "rchive"))
                error(EX_USAGE, 0, "invalid argument to -hsm-state");
            return HS_NOARCHIVE;
        case 'n':
            if (hsm_state[3] != 'e' || hsm_state[4] != 0)
                error(EX_USAGE, 0, "invalid argument to -hsm-state");
            return HS_NONE;
        case 'r':
            if (strcmp(&hsm_state[3], "elease"))
                error(EX_USAGE, 0, "invalid argument to -hsm-state");
            return HS_NORELEASE;
        default:
            error(EX_USAGE, 0, "invalid argument to -hsm-state");
            __builtin_unreachable();
        }
    case 'r':
        if (strcmp(&hsm_state[1], "eleased"))
            error(EX_USAGE, 0, "invalid argument to -hsm-state");
        return HS_RELEASED;
    }

    error(EX_USAGE, 0, "unknown hsm-state: `%s'", hsm_state);
    __builtin_unreachable();
}

struct rbh_filter *
hsm_state2filter(const char *hsm_state)
{
    enum hsm_states state = str2hsm_states(hsm_state);
    struct rbh_filter *filter;

    if (state == HS_NONE) {
        struct rbh_filter *file_filter;

        file_filter = rbh_filetype2filter("f");
        if (file_filter == NULL)
            error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                          "hsm_state2filter");

        filter = rbh_filter_exists_new(get_filter_field(LPRED_HSM_STATE));
        if (filter == NULL)
            error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                          "hsm_state2filter");

        filter = rbh_filter_not(filter);
        if (filter == NULL)
            error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                          "hsm_state2filter");

        filter = rbh_filter_and(file_filter, filter);
    } else {
        filter = rbh_filter_compare_uint32_new(
                RBH_FOP_BITS_ANY_SET, get_filter_field(LPRED_HSM_STATE), state
                );
    }

    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "hsm_state2filter");

    return filter;
}

static bool
check_balanced_braces(const char *fid)
{
    const char last = *(fid + strlen(fid) - 1);
    const char *iter = fid;
    int nb_braces = 0;

    while (*iter) {
        switch (*iter) {
        case '[':
            nb_braces++;
            break;
        case ']':
            if (nb_braces <= 0)
                return false;

            nb_braces--;
            break;
        default:
            break;
        }
        iter++;
    }

    return nb_braces == 0 &&
        (*fid == '[' ? last == ']' : true);
}

struct rbh_filter *
fid2filter(const char *fid)
{
    struct rbh_filter *filter;
    struct lu_fid lu_fid;
    char *endptr;
    int rc;

#ifdef HAVE_FID_PARSE
    rc = llapi_fid_parse(fid, &lu_fid, &endptr);
    if (rc || *endptr != '\0' || !check_balanced_braces(fid))
        error(EX_USAGE, 0, "invalid fid parsing: %s", strerror(-rc));
#else
    char *fid_format;
    int n_chars;

    if (*fid == '[')
        fid_format = "[" SFID "%n]";
    else
        fid_format = SFID "%n";

    rc = sscanf(fid, fid_format, RFID(&lu_fid), &n_chars);
    if (rc != 3)
        error(EX_USAGE, 0, "invalid fid parsing");

    switch (*fid) {
    case '[':
        if (fid[n_chars] != ']' || fid[n_chars + 1] != '\0')
            error(EX_USAGE, 0, "invalid fid parsing");
        break;
    default:
        if (fid[n_chars] != '\0')
            error(EX_USAGE, 0, "invalid fid parsing");
        break;
    }
#endif

    filter = rbh_filter_compare_binary_new(RBH_FOP_EQUAL,
                                           get_filter_field(LPRED_FID),
                                           (char *) &lu_fid, sizeof(lu_fid));
    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "fid2filter");

    return filter;
}

struct rbh_filter *
ost_index2filter(const char *ost_index)
{
    struct rbh_value index_value = {
        .type = RBH_VT_UINT64,
    };
    struct rbh_filter *filter;
    uint64_t index;
    int rc;

    if (!isdigit(*ost_index))
        error(EX_USAGE, 0, "invalid ost index: `%s'", ost_index);

    rc = str2uint64_t(ost_index, &index);
    if (rc)
        error(EX_USAGE, errno, "invalid ost index: `%s'", ost_index);

    index_value.uint64 = index;
    filter = rbh_filter_compare_sequence_new(
            RBH_FOP_IN, get_filter_field(LPRED_OST_INDEX), &index_value, 1
            );
    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "ost_index2filter");

    return filter;
}

struct rbh_filter *
mdt_index2filter(const char *mdt_index)
{
    struct rbh_filter *filter;
    uint64_t index;
    int rc;

    if (!isdigit(*mdt_index))
        error(EX_USAGE, 0, "invalid mdt index: `%s'", mdt_index);

    rc = str2uint64_t(mdt_index, &index);
    if (rc)
        error(EX_USAGE, errno, "invalid mdt index: `%s'", mdt_index);

    filter = rbh_filter_compare_uint64_new(
               RBH_FOP_EQUAL, get_filter_field(LPRED_MDT_INDEX), index);
    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "mdt_index2filter");

    return filter;
}

struct rbh_filter *
get_default_stripe_filter(void)
{
    struct rbh_filter_field default_field = {
        .fsentry = RBH_FP_INODE_XATTRS,
        .xattr = "trusted.lov",
    };
    struct rbh_filter *default_filter;
    struct rbh_filter *dir_filter;

    default_filter = rbh_filter_exists_new(&default_field);
    if (default_filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "Failed to create the default filter");

    dir_filter = rbh_filetype2filter("d");
    return rbh_filter_and(dir_filter, rbh_filter_not(default_filter));
}

static bool
get_fs_default_value(struct rbh_value_pair *pair)
{
    struct rbh_backend *backend = rbh_backend_from_uri("rbh:lustre:.");
    char *mount_path = NULL;
    char pwd[PATH_MAX];
    struct {
        int fd;
        uint16_t mode;
        struct rbh_sstack *values;
    } arg;
    int rc;

    if (backend == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "Failed to create the Lustre backend for default values retrieval");

    if (getcwd(pwd, sizeof(pwd)) == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "Failed to get the current working directory");

    rc = get_mount_path(pwd, &mount_path);
    if (rc)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "Failed to get the mount point of the current working directory '%s'",
                      pwd);

    arg.fd = open(mount_path, O_RDONLY | O_CLOEXEC);
    if (arg.fd < 0)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "Failed to open the mount point '%s' of the current working directory",
                      mount_path);

    return rbh_backend_get_attribute(backend, "dir.lov", &arg, pair, 1) == 0;
}

struct rbh_filter *
stripe_count2filter(const char *stripe_count)
{
    struct rbh_filter *default_filter;
    struct rbh_value_pair pair = {
        .key = "stripe count",
    };
    struct rbh_filter *filter;
    bool default_exists;

    default_filter = get_default_stripe_filter();
    if (strcmp(stripe_count, "default") == 0)
        return default_filter;

    default_exists = get_fs_default_value(&pair);

    filter = rbh_numeric2filter(get_filter_field(LPRED_STRIPE_COUNT),
                                stripe_count, RBH_FOP_EQUAL);
    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "Invalid stripe count provided, should be '<+|->n', got '%s'",
                      stripe_count);

    if (default_exists == false)
        return rbh_filter_and(filter, rbh_filter_not(default_filter));

    switch (filter->op) {
    case RBH_FOP_STRICTLY_LOWER:
        if (pair.value->uint64 < filter->compare.value.uint64)
            return rbh_filter_or(filter, default_filter);
        break;
    case RBH_FOP_STRICTLY_GREATER:
        if (pair.value->uint64 > filter->compare.value.uint64)
            return rbh_filter_or(filter, default_filter);
        break;
    case RBH_FOP_EQUAL:
        if (pair.value->uint64 == filter->compare.value.uint64)
            return rbh_filter_or(filter, default_filter);
        break;
    default:
        __builtin_unreachable();
    }

    return rbh_filter_and(filter, rbh_filter_not(default_filter));
}

struct rbh_filter *
stripe_size2filter(const char *stripe_size)
{
    struct rbh_filter *default_filter;
    struct rbh_value_pair pair = {
        .key = "stripe size",
    };
    struct rbh_filter *filter;
    bool default_exists;

    default_filter = get_default_stripe_filter();
    if (strcmp(stripe_size, "default") == 0)
        return default_filter;

    default_exists = get_fs_default_value(&pair);

    filter = rbh_numeric2filter(get_filter_field(LPRED_STRIPE_SIZE),
                                stripe_size, RBH_FOP_EQUAL);
    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "Invalid stripe size provided, should be '<+|->n', got '%s'",
                      stripe_size);

    if (default_exists == false)
        return rbh_filter_and(filter, rbh_filter_not(default_filter));

    switch (filter->op) {
    case RBH_FOP_STRICTLY_LOWER:
        if (pair.value->uint64 < filter->compare.value.uint64)
            return rbh_filter_or(filter, default_filter);
        break;
    case RBH_FOP_STRICTLY_GREATER:
        if (pair.value->uint64 > filter->compare.value.uint64)
            return rbh_filter_or(filter, default_filter);
        break;
    case RBH_FOP_EQUAL:
        if (pair.value->uint64 == filter->compare.value.uint64)
            return rbh_filter_or(filter, default_filter);
        break;
    default:
        __builtin_unreachable();
    }

    return rbh_filter_and(filter, rbh_filter_not(default_filter));
}

enum layout_patterns {
    LAYOUT_PATTERN_INVALID,
    LAYOUT_PATTERN_DEFAULT,
    LAYOUT_PATTERN_RAID0,
    LAYOUT_PATTERN_MDT,
    LAYOUT_PATTERN_OVERSTRIPED,
    LAYOUT_PATTERN_RELEASED
};

enum layout_patterns
str2layout_patterns(const char *layout_pattern)
{
    switch (*layout_pattern) {
    case 'd':
        if (strcmp(&layout_pattern[1], "efault") == 0)
            return LAYOUT_PATTERN_DEFAULT;
        break;
    case 'r':
        if (strcmp(&layout_pattern[1], "aid0") == 0)
            return LAYOUT_PATTERN_RAID0;
        else if (strcmp(&layout_pattern[1], "eleased") == 0)
            return LAYOUT_PATTERN_RELEASED;
        break;
    case 'm':
        if (strcmp(&layout_pattern[1], "dt") == 0)
            return LAYOUT_PATTERN_MDT;
        break;
    case 'o':
        if (strcmp(&layout_pattern[1], "verstriped") == 0)
            return LAYOUT_PATTERN_OVERSTRIPED;
        break;
    }

    return LAYOUT_PATTERN_INVALID;
}

struct rbh_filter *
layout_pattern2filter(const char *_layout)
{
    struct rbh_filter *default_filter;
    struct rbh_value_pair pair = {
        .key = "layout pattern",
    };
    enum layout_patterns layout;
    struct rbh_filter *filter;
    bool default_exists;

    layout = str2layout_patterns(_layout);
    if (layout == LAYOUT_PATTERN_INVALID)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "Invalid layout provided, should be 'raid0', 'mdt', 'overstriped' or 'default', got '%s'",
                      _layout);

    default_filter = get_default_stripe_filter();
    if (layout == LAYOUT_PATTERN_DEFAULT)
        return default_filter;

    default_exists = get_fs_default_value(&pair);

    /* The values for comparison are taken from Lustre's source code */
    switch (layout) {
    case LAYOUT_PATTERN_RAID0:
        filter = rbh_filter_compare_uint64_new(
            RBH_FOP_EQUAL,
            get_filter_field(LPRED_LAYOUT_PATTERN),
            LLAPI_LAYOUT_RAID0);
        break;
    case LAYOUT_PATTERN_MDT:
        filter = rbh_filter_compare_uint64_new(
            RBH_FOP_EQUAL,
            get_filter_field(LPRED_LAYOUT_PATTERN),
            LLAPI_LAYOUT_MDT);
        break;
    case LAYOUT_PATTERN_OVERSTRIPED:
        filter = rbh_filter_compare_uint64_new(
            RBH_FOP_EQUAL,
            get_filter_field(LPRED_LAYOUT_PATTERN),
            LLAPI_LAYOUT_OVERSTRIPING);
        break;
    case LAYOUT_PATTERN_RELEASED:
        filter = rbh_filter_compare_uint64_new(
            RBH_FOP_BITS_ANY_SET,
            get_filter_field(LPRED_LAYOUT_PATTERN),
            LOV_PATTERN_F_RELEASED);
        break;
    default:
        __builtin_unreachable();
    }

    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "Failed to create the filter for comparing a layout");

    if (default_exists == false)
        return rbh_filter_and(filter, rbh_filter_not(default_filter));

    if (pair.value->uint64 == filter->compare.value.uint64)
        return rbh_filter_or(filter, default_filter);

    return rbh_filter_and(filter, rbh_filter_not(default_filter));
}

struct rbh_filter *
expired2filter()
{
    struct rbh_filter *filter_expiration_date;
    uint64_t now;

    now = time(NULL);

    filter_expiration_date = rbh_filter_compare_uint64_new(
        RBH_FOP_LOWER_OR_EQUAL,
        get_filter_field(LPRED_EXPIRED),
        now);
    if (!filter_expiration_date)
        error(EXIT_FAILURE, errno, "rbh_filter_compare_uint64_new");

    return filter_expiration_date;
}

struct rbh_filter *
expired_at2filter(const char *expired)
{
    const struct rbh_filter_field *predicate = get_filter_field(LPRED_EXPIRED);
    struct rbh_filter *filter_expiration_date;
    struct rbh_filter *filter_inf;
    int64_t inf = INT64_MAX;

    if (!strcmp(expired, "inf")) {
        filter_inf = rbh_filter_compare_uint64_new(RBH_FOP_EQUAL, predicate,
                                                   inf);
        if (!filter_inf)
            error(EXIT_FAILURE, errno, "rbh_filter_compare_uint64_new");

        return filter_inf;
    }

    if (!isdigit(expired[0]) && expired[0] != '+' && expired[0] != '-')
        error(EXIT_FAILURE, errno, "invalid argument `%s' to `%s'", expired,
              lustre_predicate2str(LPRED_EXPIRED_AT));

    if ((expired[0] == '+' || expired[0] == '-') && !isdigit(expired[1]))
        error(EXIT_FAILURE, errno, "invalid argument `%s' to `%s'", expired,
              lustre_predicate2str(LPRED_EXPIRED_AT));

    filter_expiration_date = epoch2filter(predicate, expired);
    if (!filter_expiration_date)
        error(EXIT_FAILURE, errno, "epoch2filter");

    /* If we want to check all the entries that will be expired after a certain
     * time, do not include those that have an infinite expiration date, as it
     * internally is equivalent to an expiration date set to INT64_MAX.
     */
    filter_inf = rbh_filter_compare_uint64_new(RBH_FOP_EQUAL, predicate, inf);
    if (!filter_inf)
        error(EXIT_FAILURE, errno, "rbh_filter_compare_uint64_new");

    return rbh_filter_and(rbh_filter_not(filter_inf), filter_expiration_date);
}

struct rbh_filter *
pool2filter(const char *pool)
{
    return rbh_shell_regex2filter(get_filter_field(LPRED_POOL), pool,
                                  RBH_RO_SHELL_PATTERN);
}

struct rbh_filter *
ipool2filter(const char *pool)
{
    /* We use the same field as pool2filter because the only difference is the
     * case insensitive option.
     */
    return rbh_shell_regex2filter(get_filter_field(LPRED_POOL), pool,
                                  RBH_RO_ALL);
}

static struct rbh_filter *
comp2filter(const char *comp, const struct rbh_filter_field *field)
{
    uint64_t low_bound, high_bound;
    struct rbh_filter *filter;
    uint64_t unit_size;
    char *comma = NULL;
    uint64_t size;
    char operator;

    comma = strchr(comp, ',');
    if (comma) {
        *comma = '\0';
        rbh_get_size_parameters(comp, &operator, &unit_size, &size);
        low_bound = (size - 1) * unit_size;
        rbh_get_size_parameters(comma + 1, &operator, &unit_size, &size);
        high_bound = size * unit_size + 1;
    } else {
        rbh_get_size_parameters(comp, &operator, &unit_size, &size);
        low_bound = (size - 1) * unit_size;
        high_bound = size * unit_size + 1;
    }

    if (high_bound < low_bound) {
        uint64_t tmp = high_bound;
        high_bound = low_bound;
        low_bound = tmp;
    }

    switch (operator) {
    case '-':
        if (field == get_filter_field(LPRED_COMP_START)) {
            filter = rbh_filter_compare_uint64_new(RBH_FOP_LOWER_OR_EQUAL,
                                                   field,
                                                   (size - 1) * unit_size);
        } else {
            struct rbh_filter *left, *right;

            /* Lustre stores the component end of the last component as -1. This
             * means that if we query all entries which have a component end of
             * less than any size, we will get all the entries with components,
             * as the last one automatically has a component end of -1. To solve
             * this issue, we simply search for the entries which have a
             * component end value inferior to the requested one but superior
             * to 0 (as a file cannot have a component end of 0 anyway).
             */
            left = rbh_filter_compare_uint64_new(RBH_FOP_STRICTLY_GREATER,
                                                 field, 0);
            if (left == NULL)
                error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                              "rbh_filter_compare_uint64_new");

            right = rbh_filter_compare_uint64_new(RBH_FOP_LOWER_OR_EQUAL, field,
                                                  (size - 1) * unit_size);
            if (right == NULL)
                error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                              "rbh_filter_compare_uint64_new");

            filter = rbh_filter_array_compose(left, right);
            filter->array.field = *field;
        }

        break;
    case '+':
        filter = rbh_filter_compare_uint64_new(RBH_FOP_STRICTLY_GREATER, field,
                                               size * unit_size);
        break;
    default:
        {
            struct rbh_filter *left, *right;

            left = rbh_filter_compare_uint64_new(RBH_FOP_STRICTLY_GREATER,
                                                 field, low_bound);
            if (left == NULL)
                error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                              "rbh_filter_compare_uint64_new");

            right = rbh_filter_compare_uint64_new(RBH_FOP_STRICTLY_LOWER, field,
                                                  high_bound);
            if (right == NULL)
                error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                              "rbh_filter_compare_uint64_new");

            filter = rbh_filter_array_compose(left, right);
            filter->array.field = *field;
        }
    }

    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "filter_compare_integer");

    return filter;
}

struct rbh_filter *
comp_start2filter(const char *start)
{
    return comp2filter(start, get_filter_field(LPRED_COMP_START));
}

struct rbh_filter *
comp_end2filter(const char *end)
{
    return comp2filter(end, get_filter_field(LPRED_COMP_END));
}

struct rbh_filter *
mdt_count2filter(const char *mdt_count)
{
    return rbh_numeric2filter(get_filter_field(LPRED_MDT_COUNT), mdt_count,
                              RBH_FOP_EQUAL);
}
