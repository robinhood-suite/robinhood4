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
#include <robinhood/config.h>

struct Node
{
    char *name;
    struct Node *next;
};
typedef struct Node Node;

static int
add_list(Node **head, const char *name)
{
    Node *new_node = (Node *)malloc(sizeof(Node));

    if (new_node == NULL) {
        perror("malloc");
        return 1;
    }
    new_node->name = strdup(name);
    if (new_node->name == NULL) {
        perror("strdup");
        return 1;
    }
    new_node->next = *head;
    *head = new_node;
    return 0;
}

int
name_in_list(Node *head, const char *name) 
{
    while (head) {
        if (strcmp(head->name, name) == 0){
            return 1;
        }
        head = head->next;
    }
    return 0;
}

static const struct rbh_backend_plugin *
import_backend_plugin(const char *name)
{
    const struct rbh_backend_plugin *plugin;
    plugin = rbh_backend_plugin_import(name);

    if (plugin != NULL){
        return plugin;
    } else {
        return NULL;
    };
}

static int
capabilities_translate(const struct rbh_backend_plugin *plugin)
{
    const uint8_t capabilities = plugin->capabilities;
    printf("Capabilities of %s :\n\n",plugin->plugin.name);
    if (capabilities & RBH_FILTER_OPS)
    {
        printf("- filter\n");
    }
    if (capabilities & RBH_UPDATE_OPS)
    {
        printf("- update\n");
    }
    if (capabilities & RBH_BRANCH_OPS)
    {
        printf("- branch\n");
    }
    printf("\n");
    return 0;
}

static int
help()
{
    const char *message =
        "Usage:\n"
        "  %s <name of backend>   Show capabilities of the given backend"
        "name\n"
        "  --help                 Show this message and exit\n"
        "  --list                 Show the list of backends\n\n";
    return printf(message, program_invocation_short_name);
}

static int
search_library(const char *dir, const char *pattern, Node **head)
{
    struct stat statbuf;
    DIR *dp = opendir(dir);
    struct dirent *entry;

    if (dp == NULL) {
        return 0;
    }

    while ((entry = readdir(dp)) != NULL) {
        char path[1024];
        snprintf(path, sizeof(path),"%s/%s", dir, entry->d_name);

        if (stat(path, &statbuf) == -1) {
            continue;
        }

        if (S_ISREG(statbuf.st_mode) && strstr(entry->d_name, pattern)) {
            add_list(head, entry->d_name);
        }
    }
    closedir(dp);
    return 0;
}

static Node *
extract_names(Node *head)
{
    Node *new_node = NULL;

    while (head) {
        char *prefix_backend_name = strchr(head->name, '-');
        if (prefix_backend_name) {
            prefix_backend_name++;
        } else {
            head = head->next;
            continue;
        }

        char *suffix_backend_name = strchr(prefix_backend_name, '.');
        if (suffix_backend_name) {
            *suffix_backend_name = '\0';
        } else {
            head = head->next;
            continue;
        }

        if (name_in_list(new_node, prefix_backend_name) == 0) {
            add_list(&new_node, prefix_backend_name);
        }
        head = head->next;
    }

    return new_node;
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
    const char *prefix = "librbh-";
    struct stat statbuf;
    Node *head = NULL;

    for (int i = 0; i < len_library; i++) {
        if (lstat(library_dirs[i], &statbuf) == -1) {
            continue;
        }

        if (S_ISLNK(statbuf.st_mode)) {
            continue;
        }
        search_library(library_dirs[i], prefix, &head);
    }

    Node *backends_list = extract_names(head);

    Node *current = backends_list;
    printf("List of installed backends:\n");

    while (current != NULL) {
        printf("- %s\n", current->name);
        Node *temp = current;
        current = current->next;
        free(temp->name);
        free(temp);
    }

    while (head != NULL){
        Node *temp = head;
        head = head->next;
        free(temp->name);
        free(temp);
    }

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
    int option;

    if (argc == 1){
        printf("No backend name given, Please give a backend name\n");
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
            fprintf(stderr, "Unrecognized option\n\n");
            help();
            return EINVAL;
        }
    }

    const char *arg = argv[optind];
    const struct rbh_backend_plugin *plugin = import_backend_plugin(arg);
    if (plugin == NULL) {
        printf("This backend does not exist\n\n");
        return EINVAL;
    } else {
        capabilities_translate(plugin);
        return 0;
    }
}
