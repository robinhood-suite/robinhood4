/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <error.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wordexp.h>
#include <jansson.h>

#include "robinhood/action.h"
#include "robinhood/utils.h"
#include "robinhood/serialization.h"

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
/*
bool
rbh_action_parameters2value_map(const char *parameters,
                                struct rbh_value_map *map)
{
    yaml_parser_t parser;
    yaml_event_t event;
    char *yaml_doc;
    bool success;
    size_t len;

    if (!parameters || !map)
        return false;

    len = strlen(parameters) + 5;
    yaml_doc = xmalloc(len);
    snprintf(yaml_doc, len, "---\n%s", parameters);

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml_doc,
                                 strlen(yaml_doc));

    if (!yaml_parser_parse(&parser, &event))
        goto error;
    if (event.type != YAML_STREAM_START_EVENT) {
        yaml_event_delete(&event);
        goto error;
    }
    yaml_event_delete(&event);

    if (!yaml_parser_parse(&parser, &event))
        goto error;
    if (event.type != YAML_DOCUMENT_START_EVENT) {
        yaml_event_delete(&event);
        goto error;
    }
    yaml_event_delete(&event);

    success = parse_rbh_value_map(&parser, map, true);

    yaml_parser_delete(&parser);
    free(yaml_doc);
    return success;

error:
    yaml_parser_delete(&parser);
    free(yaml_doc);
    return false;
}*/

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
