/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <error.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wordexp.h>
#include <jansson.h>

#include "robinhood/action.h"
#include "robinhood/filters/core.h"
#include "robinhood/plugins/common_ops.h"
#include "robinhood/utils.h"
#include "robinhood/serialization.h"

const char *
action2string(enum rbh_action_type type)
{
    switch (type) {
    case RBH_ACTION_UNSET:
        return "unset";
    case RBH_ACTION_UNKNOWN:
        return "unknown";
    case RBH_ACTION_DELETE:
        return "delete";
    case RBH_ACTION_LOG:
        return "log";
    default:
        return "invalid";
    }
}

bool
rbh_action_parameters2value_map(const char *parameters,
                                struct rbh_value_map *map)
{
    yaml_parser_t parser;
    yaml_event_t event;
    bool success;

    if (!parameters || !map)
        return false;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)parameters,
                                 strlen(parameters));

    /* Skip STREAM_START and DOCUMENT_START events */
    for (int i = 0; i < 2; i++) {
        if (!yaml_parser_parse(&parser, &event)) {
            yaml_parser_delete(&parser);
            return false;
        }
        yaml_event_delete(&event);
    }

    /* Parse the map directly */
    success = parse_rbh_value_map(&parser, map, true);
    yaml_parser_delete(&parser);
    return success;
}

static size_t
count_substitutions(const char *arg)
{
    const char *cursor = arg;
    size_t count = 0;

    while ((cursor = strstr(cursor, "{}"))) {
        count++;
        cursor += 2;
    }

    return count;
}

static ssize_t
substituted_size(const char *arg, const char *substitution)
{
    size_t count;

    count = count_substitutions(arg);
    if (count == 0)
        return -1;

    return strlen(arg) + count * (strlen(substitution) - 2) + 1;
}

static char *
append_range(char *buffer, const char *start, const char *end)
{
    size_t len = end - start;

    memcpy(buffer, start, len);

    return buffer + len;
}

static char *
substitute_path(const char *arg, const char *path, char *buffer)
{
    size_t path_len = strlen(path);
    const char *prev_cursor = arg;
    const char *cursor = arg;
    char *buf_iter;

    buffer[0] = '\0';
    buf_iter = buffer;

    while ((cursor = strstr(cursor, "{}"))) {
        buf_iter = append_range(buf_iter, prev_cursor, cursor);
        buf_iter = append_range(buf_iter, path, path + path_len);

        cursor += 2;
        prev_cursor = cursor;
    }

    strcpy(buf_iter, prev_cursor);

    return buffer;
}

static const char *
rbh_action_resolve_arg(const char *arg, const char *path)
{
    ssize_t size;
    char *buffer;

    size = substituted_size(arg, path);
    if (size == -1)
        /* no "{}" found, return the original argument unchanged */
        return arg;

    buffer = xmalloc((size_t)size);
    substitute_path(arg, path, buffer);

    return buffer;
}

static int
wait_child(pid_t child)
{
    int status;
    int rc;

    rc = waitpid(child, &status, 0);
    if (rc == -1) {
        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    return -1;
}

int
rbh_action_exec_argv(const char **argv, const char *path)
{
    size_t argc = 0;
    const char **resolved;
    pid_t child;
    int rc;

    /* Count arguments */
    while (argv[argc])
        argc++;

    /* Resolve "{}" placeholders in each token */
    resolved = xcalloc(argc + 1, sizeof(*resolved));
    for (size_t i = 0; i < argc; i++)
        resolved[i] = rbh_action_resolve_arg(argv[i], path);
    resolved[argc] = NULL;

    child = fork();

    switch (child) {
    case -1:
        perror("fork");
        rc = -1;
        break;
    case 0:
        /* Child process */
        execvp(resolved[0], (char *const *)resolved);
        error(EXIT_FAILURE, errno, "failed to execute '%s'", resolved[0]);
        __builtin_unreachable();
    default:
        /* Parent process */
        rc = wait_child(child);
        break;
    }

    /* Free only the resolved arguments that were allocated */
    for (size_t i = 0; i < argc; i++) {
        if (resolved[i] != argv[i])
            free((char *)resolved[i]);
    }
    free(resolved);

    return rc;
}

int
rbh_action_exec_command(const char *cmd_str, const char *path)
{
    wordexp_t we;
    int rc;

    rc = wordexp(cmd_str, &we, WRDE_NOCMD | WRDE_SHOWERR);
    if (rc != 0) {
        fprintf(stderr, "CmdAction | failed to parse command: '%s'\n",
                cmd_str);
        errno = EINVAL;
        return -1;
    }

    rc = rbh_action_exec_argv((const char **)we.we_wordv, path);

    wordfree(&we);

    return rc;
}

static int
rbh_action_print_escape(char *output, int max_length, const char *escape)
{
    assert(escape != NULL);
    assert(*escape != '\0');

    (void) max_length;

    /* For now, consider the escape to be a single character */
    switch (*escape) {
    case 'n':
        *output = '\n';
        return 1;
    default:
        error(EXIT_FAILURE, ENOTSUP, "format escape not supported");
    }

    __builtin_unreachable();
}

static int
rbh_action_print_regular_char(char *output, int max_length,
                              const char *format_string)
{
    int sublength = 0;

    assert(format_string != NULL);
    assert(*format_string != '\0');

    while (format_string[sublength] != '%' &&
           format_string[sublength] != '\\' &&
           format_string[sublength] != '\0' &&
           sublength < max_length) {
        output[sublength] = format_string[sublength];
        sublength++;
    }

    return sublength;
}

int
rbh_action_format_fsentry(const char *format_string,
                          const struct filters_context *f_ctx,
                          const struct rbh_fsentry *fsentry,
                          const char *backend,
                          char *output,
                          size_t output_size)
{
    const char *backend_name = backend ? backend : "";
    size_t output_length = 0;
    int max_length;
    size_t length;

    if (!format_string || !f_ctx || !fsentry || !output || output_size == 0) {
        errno = EINVAL;
        return -1;
    }

    output[0] = '\0';
    length = strlen(format_string);
    max_length = (int)output_size;

    for (size_t i = 0; i < length; i++) {
        int tmp_length = 0;

        if (i + 1 >= length) {
            output[output_length] = format_string[i];
            tmp_length = 1;
            goto finish;
        }

        switch (format_string[i]) {
        case '%':
            if (format_string[i + 1] == '%') {
                output[output_length] = '%';
                tmp_length = 1;
                i++;
                break;
            }

            for (size_t j = 0; j < f_ctx->info_pe_count; ++j) {
                const struct rbh_pe_common_operations *ops;

                ops = get_common_operations(&f_ctx->info_pe[j]);
                if (!ops || !ops->fill_entry_info)
                    continue;

                tmp_length = rbh_pe_common_ops_fill_entry_info(
                    ops,
                    output + output_length,
                    max_length,
                    fsentry,
                    format_string + i + 1,
                    backend_name
                );

                if (tmp_length > 0)
                    break;
            }

            i++;
            break;
        case '\\':
            tmp_length = rbh_action_print_escape(output + output_length,
                                                 max_length,
                                                 format_string + i + 1);
            i++;
            break;
        default:
            tmp_length = rbh_action_print_regular_char(output + output_length,
                                                       max_length,
                                                       format_string + i);
            i += (size_t)tmp_length - 1;
            break;
        }

finish:

        if (tmp_length < 0) {
            errno = EINVAL;
            return -1;
        }

        output_length += (size_t)tmp_length;
        max_length -= tmp_length;

        if (max_length <= 0) {
            output_length = output_size - 1;
            break;
        }
    }

    output[output_length] = '\0';

    return (int)output_length;
}
