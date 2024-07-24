/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */
#include <dlfcn.h>
#include <errno.h>
#include <error.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sysexits.h>
#include <getopt.h>

#include <robinhood.h>
#include <robinhood/utils.h>
#include <robinhood/list.h>
#include <robinhood/config.h>

#define LIB_RBH_PREFIX "librbh-"

struct rbh_node_capabilities {
    char *name;
    struct rbh_list_node list;
};

static int
add_list(struct rbh_list_node *head, const char *name)
{
    struct rbh_node_capabilities *new_node = malloc(sizeof(*new_node));

    if (new_node == NULL) {
        perror("malloc");
        return 1;
    }
    new_node->name = strdup(name);
    if (new_node->name == NULL) {
        perror("strdup");
        return 1;
    }
    rbh_list_add_tail(head, &new_node->list);
    return 0;
}

static bool
name_in_list(struct rbh_list_node *head, const char *name)
{
    struct rbh_node_capabilities *node;

    if (rbh_list_empty(head)) {
        return true;
    }

    rbh_list_foreach(head, node, list) {
        if (strcmp(node->name, name) == 0)
            return false;
    }
    return true;
}

static int
capabilities_translate(const struct rbh_backend_plugin *plugin)
{
    const uint8_t capabilities = plugin->capabilities;

    printf("Capabilities of %s:\n",plugin->plugin.name);
    if (capabilities & RBH_FILTER_OPS)
        printf("- filter: rbh-find [source]\n");
    if (capabilities & RBH_READ_OPS)
        printf("- synchronisation: rbh-sync [source]\n");
    if (capabilities & RBH_UPDATE_OPS)
        printf("- update: rbh-sync [target]\n");
    if (capabilities & RBH_BRANCH_OPS)
        printf("- branch: rbh-sync [source for partial processing]\n");

    return 0;
}

static int
help()
{
    const char *message =
        "Usage:"
        "  %s <name of backend>   Show capabilities of the given backend"
        " name\n"
        "Arguments:\n"
        "  -h --help                 Show this message and exit\n"
        "  -l --list                 Show the list of backends\n"
        "Backends capabilities list:\n"
        "- filter: The ability to read the data after filtering it according to"
        " different criteria\n"
        "- synchronisation: The ability to read the data\n"
        "- update: The ability to update information or metadata of files in"
        " the backend\n"
        "- branch: The ability to read data over a subsection of a backend\n";
    return printf(message, program_invocation_short_name);
}

static int
search_library(const char *dir, const char *prefix, struct rbh_list_node *head)
{
    DIR *dp = opendir(dir);
    struct dirent *entry;
    struct stat statbuf;

    if (dp == NULL)
        return 0;

    while ((entry = readdir(dp)) != NULL) {
        char path[1024];
        snprintf(path, sizeof(path),"%s/%s", dir, entry->d_name);

        if (stat(path, &statbuf) == -1)
            continue;

        if (S_ISREG(statbuf.st_mode) && strstr(entry->d_name, prefix))
            add_list(head, entry->d_name);
    }
    closedir(dp);
    return 0;
}

static struct rbh_list_node *
extract_names(struct rbh_list_node *head)
{
    struct rbh_list_node *new_list = malloc(sizeof(*new_list));
    struct rbh_node_capabilities *node;
    struct rbh_node_capabilities *tmp;

    if (new_list == NULL) {
        perror("malloc");
        return NULL;
    }

    rbh_list_init(new_list);

    rbh_list_foreach_safe(head, node, tmp, list) {
        char *prefix_backend_name = strchr(node->name, '-');
        if (prefix_backend_name)
            prefix_backend_name++;
        else
            continue;

        char *suffix_backend_name = strchr(prefix_backend_name, '.');
        if (suffix_backend_name)
            *suffix_backend_name = '\0';
        else
            continue;

        if (name_in_list(new_list, prefix_backend_name) == true) {
            if (add_list(new_list, prefix_backend_name) != 0) {
                free(new_list);
                return NULL;
            }
        }
    }

    return new_list;
}

static int
check_ld_library_path(const char *pattern, struct rbh_list_node *head)
{
    const char *env = getenv("LD_LIBRARY_PATH");
    if (env == NULL)
        return 0;

    char *ld_library_path = strdup(env);
    if (ld_library_path == NULL) {
        perror("strdup");
        return 0;
    }

    char *path = strtok(ld_library_path, ":");
    while (path != NULL) {
        search_library(path, pattern, head);
        path = strtok(NULL, ":");
    }

    free(ld_library_path);
    return 1;
}

static int
rbh_backend_list()
{
    const char *library_dirs[] = {
        "/lib",
        "/usr/lib",
        "/usr/local/lib",
        "/lib64",
        "/usr/lib64",
        "/usr/local/lib64",
    };
    int len_library = sizeof(library_dirs) / sizeof(library_dirs[0]);
    struct rbh_list_node *backends_list;
    struct rbh_node_capabilities *node;
    struct rbh_node_capabilities *tmp;
    struct rbh_list_node head;

    rbh_list_init(&head);

    if (check_ld_library_path(prefix, &head) && !rbh_list_empty(&head)) {
        backends_list = extract_names(&head);
        if (backends_list == NULL) {
            return 1;
        }

        printf("List of installed backends:\n");

        rbh_list_foreach_safe(backends_list, node, tmp, list) {
            printf("- %s\n", node->name);
            free(node->name);
            free(node);
        }

        rbh_list_foreach_safe(&head, node, tmp, list) {
            free(node->name);
            free(node);
        }

        free(backends_list);
        return 0;
    }

    for (int i = 0; i < len_library; i++) {
        struct stat statbuf;
        if (lstat(library_dirs[i], &statbuf) == -1)
            continue;

        if (S_ISLNK(statbuf.st_mode))
            continue;

        search_library(library_dirs[i], LIB_RBH_PREFIX, &head);
    }

    backends_list = extract_names(&head);
    if (backends_list == NULL) {
        return 1;
    }

    printf("List of installed backends:\n");

    rbh_list_foreach_safe(backends_list, node, tmp, list) {
        printf("- %s\n", node->name);
        free(node->name);
        free(node);
    }

    rbh_list_foreach_safe(&head, node, tmp, list) {
        free(node->name);
        free(node);
    }

    free(backends_list);

    return 0;
}

int
main(int argc, char **argv)
{
    const struct option LONG_OPTIONS[] = {
        {
            .name = "help",
            .val = 'h'
        },
        {
            .name = "list",
            .val = 'l'
        },
        {}
    };
    const struct rbh_backend_plugin *plugin;
    const char *arg;
    int option;

    if (argc == 1){
        fprintf(stderr, "No backend name given, Please give a backend name\n");
        help();
        return EINVAL;
    }

    while ((option = getopt_long(argc, argv, "hl", LONG_OPTIONS,
                                 NULL)) != -1) {
        switch (option) {
        case 'h':
            help();
            return 0;
        case 'l':
            rbh_backend_list();
            return 0;
        default :
            fprintf(stderr, "Unrecognized option\n");
            help();
            return EINVAL;
        }
    }

    arg = argv[optind];
    plugin = rbh_backend_plugin_import(arg);

    if (plugin == NULL) {
        fprintf(stderr, "This backend does not exist\n");
        return EINVAL;
    } else {
        capabilities_translate(plugin);
        return 0;
    }
}
