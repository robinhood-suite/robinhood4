/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <robinhood/list.h>
#include <robinhood/utils.h>

#include "info.h"

#define LIB_RBH_PREFIX "librbh-"

struct rbh_node_info {
    char *name;
    struct rbh_list_node list;
};

static int
add_list(struct rbh_list_node *head, const char *name)
{
    struct rbh_node_info *new_node = xmalloc(sizeof(*new_node));

    new_node->name = xstrdup(name);
    rbh_list_add_tail(head, &new_node->list);

    return 0;
}

static bool
is_name_in_list(struct rbh_list_node *head, const char *name)
{
    struct rbh_node_info *node;

    if (rbh_list_empty(head))
        return false;

    rbh_list_foreach(head, node, list) {
        if (strcmp(node->name, name) == 0)
            return true;
    }

    return false;
}

static int
search_library(const char *dir, const char *prefix, struct rbh_list_node *head)
{
    DIR *dp = opendir(dir);
    struct dirent *entry;

    if (dp == NULL)
        return 0;

    while ((entry = readdir(dp)) != NULL)
        if (entry->d_type == DT_REG && strstr(entry->d_name, prefix))
            if (is_name_in_list(head, entry->d_name) == false)
                add_list(head, entry->d_name);

    closedir(dp);

    return 0;
}

static void
parse_plugin_and_extension(char *backend_name, char **plugin, char **extension)
{
    char *ext = strrchr(backend_name, '-');

    if (ext && strcmp(ext + 1, "ext") == 0) {
        char *last_part;

        *ext = '\0';
        last_part = strrchr(backend_name, '-');
        *extension = last_part + 1;
        *last_part = '\0';
    } else {
        *extension = NULL;
    }

    *plugin = backend_name;
}

static void
print_backend_list(struct rbh_list_node *head)
{
    struct rbh_node_info *node;
    struct rbh_node_info *tmp;

    printf("List of installed plugins and their extensions:\n");

    rbh_list_foreach_safe(head, node, tmp, list) {
        char *backend_name = node->name + strlen(LIB_RBH_PREFIX);
        char *suffix_backend_name = strchr(backend_name, '.');
        const struct rbh_backend_plugin *backend_plugin;
        char *extension;
        char *plugin;

        assert(suffix_backend_name);
        *suffix_backend_name = '\0';

        parse_plugin_and_extension(backend_name, &plugin, &extension);

        backend_plugin = rbh_backend_plugin_import(plugin);
        if (backend_plugin == NULL) {
            fprintf(stderr, "Failed to import plugin '%s'\n", plugin);
            continue;
        }

        if (extension == NULL)
            printf("- %s\n", plugin);
        else if (rbh_plugin_load_extension(&backend_plugin->plugin, extension))
            printf("    - %s\n", extension);

        free(node->name);
        free(node);
    }
}

static int
check_ld_library_path(const char *pattern, struct rbh_list_node *head)
{
    const char *env = getenv("LD_LIBRARY_PATH");
    char *ld_library_path;
    char *path;

    if (env == NULL)
        return 0;

    ld_library_path = xstrdup(env);

    path = strtok(ld_library_path, ":");
    while (path != NULL) {
        search_library(path, pattern, head);
        path = strtok(NULL, ":");
    }

    free(ld_library_path);
    return 1;
}

void
list_plugins_and_extensions()
{
    struct rbh_list_node *head = xmalloc(sizeof(struct rbh_list_node));
    const char *library_dirs[] = {
        "/lib",
        "/usr/lib",
        "/lib64",
        "/usr/lib64",
    };
    int len_library;

    len_library = sizeof(library_dirs) / sizeof(library_dirs[0]);
    rbh_list_init(head);

    if (check_ld_library_path(LIB_RBH_PREFIX, head) && !rbh_list_empty(head)) {
        // FIXME We don't go through library_dirs if LD_LIBRARY_PATH is set
        print_backend_list(head);
        free(head);
        return;
    }

    for (int i = 0; i < len_library; i++) {
        struct stat statbuf;
        if (lstat(library_dirs[i], &statbuf) == -1)
            continue;

        if (S_ISLNK(statbuf.st_mode))
            continue;

        search_library(library_dirs[i], LIB_RBH_PREFIX, head);
    }

    print_backend_list(head);
    free(head);
}
