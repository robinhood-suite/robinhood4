/* This file is part of rbh-find
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <errno.h>
#include <error.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <ctype.h>

#include <sys/stat.h>
#ifndef HAVE_STATX
# include <robinhood/statx.h>
#endif

#include <robinhood/backend.h>
#include <robinhood/sstack.h>

#include "filters.h"
#include "utils.h"

static const struct rbh_filter_field predicate2filter_field[] = {
    [PRED_AMIN]     = { .fsentry = RBH_FP_STATX, .statx = STATX_ATIME, },
    [PRED_ATIME]    = { .fsentry = RBH_FP_STATX, .statx = STATX_ATIME, },
    [PRED_CMIN]     = { .fsentry = RBH_FP_STATX, .statx = STATX_CTIME, },
    [PRED_CTIME]    = { .fsentry = RBH_FP_STATX, .statx = STATX_CTIME, },
    [PRED_NAME]     = { .fsentry = RBH_FP_NAME, },
    [PRED_INAME]    = { .fsentry = RBH_FP_NAME, },
    [PRED_MMIN]     = { .fsentry = RBH_FP_STATX, .statx = STATX_MTIME, },
    [PRED_MTIME]    = { .fsentry = RBH_FP_STATX, .statx = STATX_MTIME, },
    [PRED_TYPE]     = { .fsentry = RBH_FP_STATX, .statx = STATX_TYPE },
    [PRED_SIZE]     = { .fsentry = RBH_FP_STATX, .statx = STATX_SIZE },
    [PRED_PERM]     = { .fsentry = RBH_FP_STATX, .statx = STATX_MODE },
};

struct rbh_filter *
shell_regex2filter(enum predicate predicate, const char *shell_regex,
                   unsigned int regex_options)
{
    struct rbh_filter *filter;
    char *pcre;

    pcre = shell2pcre(shell_regex);
    if (pcre == NULL)
        error_at_line(EXIT_FAILURE, ENOMEM, __FILE__, __LINE__ - 2,
                      "converting %s into a Perl Compatible Regular Expression",
                      shell_regex);

    filter = rbh_filter_compare_regex_new(RBH_FOP_REGEX,
                                          &predicate2filter_field[predicate],
                                          pcre, regex_options);
    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__ - 2,
                      "building a regex filter for %s", pcre);
    free(pcre);
    return filter;
}

static struct rbh_filter *
filter_uint64_range_new(const struct rbh_filter_field *field, uint64_t start,
                        uint64_t end)
{
    struct rbh_filter *low, *high;

    low = rbh_filter_compare_uint64_new(RBH_FOP_STRICTLY_GREATER, field, start);
    if (low == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_filter_compare_time");

    high = rbh_filter_compare_uint64_new(RBH_FOP_STRICTLY_LOWER, field, end);
    if (high == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_filter_compare_time");

    return filter_and(low, high);
}

static struct rbh_filter *
timedelta2filter(enum predicate predicate, enum time_unit unit,
                 const char *_timedelta)
{
    const struct rbh_filter_field *field = &predicate2filter_field[predicate];
    const char *timedelta = _timedelta;
    char operator = *timedelta;
    struct rbh_filter *filter;
    unsigned long delta; /* in seconds */
    time_t now, then;
    int save_errno;

    switch (operator) {
    case '-':
    case '+':
        timedelta++;
    }

    /* Convert the time string to a number of seconds */
    save_errno = errno;
    errno = 0;
    delta = str2seconds(unit, timedelta);
    if ((errno == ERANGE && delta == ULONG_MAX) || (errno != 0 && delta == 0))
        error(EXIT_FAILURE, 0, "invalid argument `%s' to `%s'", _timedelta,
              predicate2str(predicate));
    errno = save_errno;

    /* Compute `then' */
    now = time(NULL);
    if (now < 0)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__, "time");
    then = now - delta;

    switch (operator) {
    case '-':
        filter = rbh_filter_compare_uint64_new(RBH_FOP_STRICTLY_GREATER, field,
                                              then);
        if (filter == NULL)
            error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                          "rbh_filter_compare_time_new");
        break;
    case '+':
        filter = rbh_filter_compare_uint64_new(RBH_FOP_STRICTLY_LOWER, field,
                                               then);
        if (filter == NULL)
            error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                          "rbh_filter_compare_time_new");
        break;
    default:
        filter = filter_uint64_range_new(field, then - TIME_UNIT2SECONDS[unit],
                                         then);
        if (filter == NULL)
            error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                          "filter_time_range_new");
        break;
    }

    return filter;
}

struct rbh_filter *
xmin2filter(enum predicate predicate, const char *minutes)
{
    return timedelta2filter(predicate, TU_MINUTE, minutes);
}

struct rbh_filter *
xtime2filter(enum predicate predicate, const char *days)
{
    return timedelta2filter(predicate, TU_DAY, days);
}

struct rbh_filter *
filetype2filter(const char *_filetype)
{
    struct rbh_filter *filter;
    int filetype;

    if (_filetype[0] == '\0' || _filetype[1] != '\0')
        error(EX_USAGE, 0, "arguments to -type should only contain one letter");

    switch (_filetype[0]) {
    case 'b':
        filetype = S_IFBLK;
        break;
    case 'c':
        filetype = S_IFCHR;
        break;
    case 'd':
        filetype = S_IFDIR;
        break;
    case 'f':
        filetype = S_IFREG;
        break;
    case 'l':
        filetype = S_IFLNK;
        break;
    case 'p':
        filetype = S_IFIFO;
        break;
    case 's':
        filetype = S_IFSOCK;
        break;
    default:
        error(EX_USAGE, 0, "unknown argument to -type: %s", _filetype);
        /* clang: -Wsometimes-unitialized: `filtetype` */
        __builtin_unreachable();
    }

    filter = rbh_filter_compare_int32_new(RBH_FOP_EQUAL,
                                          &predicate2filter_field[PRED_TYPE],
                                          filetype);
    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "filter_compare_integer");

    return filter;
}

struct rbh_filter *
filesize2filter(const char *_filesize)
{
    char operator = *_filesize;
    struct rbh_filter *filter;
    uint64_t unit_size;
    uint64_t filesize;
    char *suffix;

    switch (operator) {
    case '+':
    case '-':
        _filesize++;
    }

    filesize = strtoull(_filesize, &suffix, 10);
    if (filesize == 0ULL)
        error(EX_USAGE, 0,
              "arguments to -size should start with at least one digit");
    else if (errno == ERANGE && filesize == ULLONG_MAX)
        error(EX_USAGE, EOVERFLOW, "invalid argument `%s' to -size", _filesize);

    switch (*suffix++) {
    case 'T':
        unit_size = 1099511627776;
        break;
    case 'G':
        unit_size = 1073741824;
        break;
    case 'M':
        unit_size = 1048576;
        break;
    case 'k':
        unit_size = 1024;
        break;
    case '\0':
        /* default suffix */
        suffix--;
        __attribute__((fallthrough));
    case 'b':
        unit_size = 512;
        break;
    case 'w':
        unit_size = 2;
        break;
    case 'c':
        unit_size = 1;
        break;
    default:
        error(EX_USAGE, 0, "invalid argument `%s' to -size", _filesize);
    }

    if (*suffix)
        error(EX_USAGE, 0, "invalid argument `%s' to -size", _filesize);

    switch (operator) {
    case '-':
        filter = rbh_filter_compare_uint64_new(
                RBH_FOP_LOWER_OR_EQUAL, &predicate2filter_field[PRED_SIZE],
                (filesize - 1) * unit_size
                );
        break;
    case '+':
        filter = rbh_filter_compare_uint64_new(
                RBH_FOP_STRICTLY_GREATER, &predicate2filter_field[PRED_SIZE],
                filesize * unit_size
                );
        break;
    default:
        filter = filter_uint64_range_new(&predicate2filter_field[PRED_SIZE],
                                         (filesize - 1) * unit_size,
                                         filesize * unit_size + 1);
    }

    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "filter_compare_integer");

    return filter;
}

struct who {
    bool u, o, g;
};

static bool
who_is_empty(const struct who *who)
{
    return !(who->u || who->g || who->o);
}

static void
parse_symbolic_who(const char **input, struct who *who)
{
    while (true) {
        switch (**input) {
        case 'u':
            who->u = true;
            break;
        case 'g':
            who->g = true;
            break;
        case 'o':
            who->o = true;
            break;
        case 'a':
            who->u = true;
            who->g = true;
            who->o = true;
            break;
        default:
            return;
        }
        (*input)++;
    }
}

static mode_t
parse_symbolic_perm(const struct who *who, const char **input, mode_t mode)
{
    bool empty_who = who_is_empty(who);
    bool all = (who->u && who->g && who->o);
    mode_t perm = 0;

    while (true) {
        switch (**input) {
        case 'r':
            perm |= who->u ? 0400 : 0;
            perm |= who->g ? 0040 : 0;
            perm |= who->o ? 0004 : 0;
            perm |= empty_who ? 0444 : 0;
            break;
        case 'w':
            perm |= who->u ? 0200 : 0;
            perm |= who->g ? 0020 : 0;
            perm |= who->o ? 0002 : 0;
            perm |= empty_who ? 0222 : 0;
            break;
        case 'x':
            perm |= who->u ? 0100 : 0;
            perm |= who->g ? 0010 : 0;
            perm |= who->o ? 0001 : 0;
            perm |= empty_who ? 0111 : 0;
            break;
        case 'X':
            /* Adds execute permission to 'u', 'g' and/or 'o' if specified and
             * either 'u', 'g' or 'o' already has execute permissions.
             */
            if ((mode & 0111) != 0) {
                perm |= who->u ? 0100 : 0;
                perm |= who->g ? 0010 : 0;
                perm |= who->o ? 0001 : 0;
            }
            break;
        case 's':
            /* 's' is ignored if only 'o' is given, it's not an error */
            if (who->o && !who->g && !who->u)
                break;
            perm |= who->u ? S_ISUID : 0;
            perm |= who->g ? S_ISGID : 0;
            break;
        case 't':
            /* 't' should be used when 'o' or 'a' is given or who is empty */
            perm |= (who->o || empty_who || all) ? S_ISVTX : 0;
            break;
        default:
            return perm;
        }
        (*input)++;
    }
}

static mode_t
parse_symbolic_permcopy(const struct who *who, const char **input, mode_t mode)
{
    bool empty_who = who_is_empty(who);
    mode_t previous_flags = 0;
    mode_t perm = 0;

    switch (**input) {
    case 'u':
        previous_flags = (mode & 0700);
        perm |= (empty_who || who->u) ? previous_flags : 0;
        perm |= (empty_who || who->g) ? (previous_flags >> 3) : 0;
        perm |= (empty_who || who->o) ? (previous_flags >> 6) : 0;
        (*input)++;
        break;
    case 'g':
        previous_flags = (mode & 0070);
        perm |= (empty_who || who->u) ? (previous_flags << 3) : 0;
        perm |= (empty_who || who->g) ? previous_flags : 0;
        perm |= (empty_who || who->o) ? (previous_flags >> 3) : 0;
        (*input)++;
        break;
    case 'o':
        previous_flags = (mode & 0007);
        perm |= (empty_who || who->u) ? (previous_flags << 6) : 0;
        perm |= (empty_who || who->g) ? (previous_flags << 3) : 0;
        perm |= (empty_who || who->o) ? previous_flags : 0;
        (*input)++;
        break;
    default:
        break;
    }

    return perm;
}

static bool
is_op(char input)
{
    return (input == '-' || input == '+' || input == '=');
}

static mode_t
symbolic_action(const struct who *who, char op, mode_t current, mode_t new)
{
    switch (op) {
    case '-':
        /* remove the flags from mode */
        return current & ~new;
    case '+':
        /* add the flags to mode */
        return current | new;
    case '=':
        /* set the flags of mode to perm */
        if (new != 0) {
            if (who_is_empty(who))
                return new;

            if (who->u)
                current = ((new & 0700) | (current & 0077));
            if (who->g)
                current = ((new & 0070) | (current & 0707));
            if (who->o)
                current = ((new & 0007) | (current & 0770));
            return current;
        }
        return 0;
    default:
        return 017777;
    }
}

static unsigned long
octal_str2mode(const char *input, const char **end)
{
    unsigned long mode;
    char *c;

    mode = strtoul(input, &c, 8);
    if (mode > 07777 || (*c != '\0' && *c != ','))
        return ULONG_MAX;

    if (end)
        *end = c;


    return mode;
}

static mode_t
parse_symbolic_actionlist(const struct who *who, mode_t input_mode,
                          const char **input)
{
    bool empty_who = who_is_empty(who);
    mode_t perm = input_mode;
    char op;

    if (!is_op(**input))
        return 17777;

    while (is_op(**input)) {
        mode_t tmp = 0;

        op = *(*input)++;
        if (empty_who && **input == '\0')
            return 017777;

        tmp = parse_symbolic_permcopy(who, input, perm);
        if (tmp == 0) {
            tmp = parse_symbolic_perm(who, input, perm);

            if (tmp > 07777) {
                return tmp;
            }
        }

        perm = symbolic_action(who, op, perm, tmp);
    }

    return perm;
}

static mode_t
parse_symbolic_clause(const char *input, mode_t current, const char **end)
{
    struct who who = {
        .u = false,
        .g = false,
        .o = false
    };
    mode_t mode;

    parse_symbolic_who(&input, &who);

    if (who_is_empty(&who)) {
        mode_t tmp;
        char op;

        op = *input++;
        tmp = octal_str2mode(input, end);

        if (tmp <= 07777) {
            tmp = symbolic_action(&who, op, current, tmp);
            return tmp;
        }

        input--;
    }

    mode = parse_symbolic_actionlist(&who, current, &input);
    *end = input;

    return mode;
}

static mode_t
symbolic_str2mode(const char *input)
{
    /* parse comma seperated list of symbolic representation */
    const char *end = NULL;
    mode_t mode = 0;

    do {
        if (*input == '\0')
            return 017777;

        mode = parse_symbolic_clause(input, mode, &end);

        if (mode > 07777)
            return mode;
        input = end+1;
    } while (*end == ',');

    if (*end != '\0')
        return 017777;

    return mode;
}

static unsigned long
str2mode(const char *input)
{
    switch (*input) {
    case '0' ... '7':
        return octal_str2mode(input, NULL);
    case '8' ... '9':
        return ULONG_MAX;
    default:
        return symbolic_str2mode(input);
    }
}

struct rbh_filter *
mode2filter(const char *_input)
{
    enum rbh_filter_operator operator;
    struct rbh_filter *filter;
    const char *input = _input;
    unsigned long mode;

    if (*input == '\0')
        error(EX_USAGE, 0,
              "arguments to -perm should contain at least one digit or a symbolic mode");

    switch (*input) {
    case '/':
        operator = RBH_FOP_BITS_ANY_SET;
        input++;
        break;
    case '-':
        operator = RBH_FOP_BITS_ALL_SET;
        input++;
        break;
    default:
        operator = RBH_FOP_EQUAL;
        break;
    }

    mode = str2mode(input);
    if (mode > 07777)
        error(EX_USAGE, 0, "invalid mode: %s", _input);

    filter = rbh_filter_compare_uint32_new(operator,
                                           &predicate2filter_field[PRED_PERM],
                                           mode);
    if (filter == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "filter_compare_uint32_new");

    return filter;
}

static struct rbh_sstack *filters;

static void __attribute__((constructor))
init_filters(void)
{
    const int MIN_FILTER_ALLOC = 32;

    filters = rbh_sstack_new(MIN_FILTER_ALLOC * sizeof(struct rbh_filter *));
    if (filters == NULL)
        error(EXIT_FAILURE, errno, "rbh_sstack_new");
}

static void __attribute__((destructor))
exit_filters(void)
{
    struct rbh_filter **filter;
    size_t size;

    while (true) {
        int rc;

        filter = rbh_sstack_peek(filters, &size);
        if (size == 0)
            break;

        assert(size % sizeof(*filter) == 0);

        for (size_t i = 0; i < size / sizeof(*filter); i++, filter++)
            free(*filter);

        rc = rbh_sstack_pop(filters, size);
        assert(rc == 0);
    }
    rbh_sstack_destroy(filters);
}

static struct rbh_filter *
filter_compose(enum rbh_filter_operator op, struct rbh_filter *left,
               struct rbh_filter *right)
{
    const struct rbh_filter **array;
    struct rbh_filter *filter;

    assert(op == RBH_FOP_AND || op == RBH_FOP_OR);

    filter = malloc(sizeof(*filter));
    if (filter == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    array = rbh_sstack_push(filters, NULL, sizeof(*array) * 2);
    if (array == NULL)
        error(EXIT_FAILURE, errno, "rbh_sstack_push");

    array[0] = left;
    array[1] = right;

    filter->op = op;
    filter->logical.filters = array;
    filter->logical.count = 2;

    return filter;
}

struct rbh_filter *
filter_and(struct rbh_filter *left, struct rbh_filter *right)
{
    return filter_compose(RBH_FOP_AND, left, right);
}

struct rbh_filter *
filter_or(struct rbh_filter *left, struct rbh_filter *right)
{
    return filter_compose(RBH_FOP_OR, left, right);
}

struct rbh_filter *
filter_not(struct rbh_filter *filter)
{
    struct rbh_filter *not;

    not = malloc(sizeof(*not));
    if (not == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    not->logical.filters = rbh_sstack_push(filters, &filter, sizeof(filter));
    if (not->logical.filters == NULL)
        error(EXIT_FAILURE, errno, "rbh_sstack_push");

    not->op = RBH_FOP_NOT;
    not->logical.count = 1;

    return not;
}

struct rbh_filter_field
str2field(const char *attribute)
{
    struct rbh_filter_field field;

    switch(attribute[0]) {
    case 'a':
        if (strcmp(&attribute[1], "time") == 0) {
            field.fsentry = RBH_FP_STATX;
            field.statx = STATX_ATIME;
            return field;
        }
        break;
    case 'b':
        if (strcmp(&attribute[1], "locks") == 0) {
            field.fsentry = RBH_FP_STATX;
            field.statx = STATX_BLOCKS;
            return field;
        }
        break;
    case 'c':
        if (strcmp(&attribute[1], "time") == 0) {
            field.fsentry = RBH_FP_STATX;
            field.statx = STATX_CTIME;
            return field;
        }
        break;
    case 'i':
        if (strcmp(&attribute[1], "no") == 0) {
            field.fsentry = RBH_FP_STATX;
            field.statx = STATX_INO;
            return field;
        }
        break;
    case 'm':
        if (strcmp(&attribute[1], "time") == 0) {
            field.fsentry = RBH_FP_STATX;
            field.statx = STATX_MTIME;
            return field;
        }
        break;
    case 'n':
        if (strcmp(&attribute[1], "ame") == 0) {
            field.fsentry = RBH_FP_NAME;
            return field;
        }
        break;
    case 's':
        if (strcmp(&attribute[1], "ize") == 0) {
            field.fsentry = RBH_FP_STATX;
            field.statx = STATX_SIZE;
            return field;
        }
        break;
    case 't':
        if (strcmp(&attribute[1], "ype") == 0) {
            field.fsentry = RBH_FP_STATX;
            field.statx = STATX_TYPE;
            return field;
        }
        break;
    }
    error(EX_USAGE, 0, "invalid field for sort: %s", attribute);
    __builtin_unreachable();
}

struct rbh_filter_sort *
sort_options_append(struct rbh_filter_sort *sorts, size_t count,
                    struct rbh_filter_field field, bool ascending)
{
    struct rbh_filter_sort *tmp = sorts;
    struct rbh_filter_sort sort = {
        .field = field,
        .ascending = ascending
    };

    tmp = reallocarray(tmp, count + 1, sizeof(*tmp));
    if (tmp == NULL)
        error(EXIT_FAILURE, 0, "reallocarray with rbh_filter_sort");
    tmp[count] = sort;

    return tmp;
}
