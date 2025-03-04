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
#define RBH_SIZE_FLAG 0x00000001U
#define RBH_FIRST_SYNC_FLAG 0x00000002U
#define RBH_LAST_SYNC_FLAG 0x00000004U
#define RBH_CAPABILITIES_FLAG 0x00000008U

static struct rbh_backend *from;

static void __attribute__((destructor))
destroy_from(void)
{
    const char *name;

    if(from) {
        name = from->name;
        rbh_backend_destroy(from);
        rbh_backend_plugin_destroy(name);
    }
}

struct rbh_node_info {
    char *name;
    struct rbh_list_node list;
};

static int
add_list(struct rbh_list_node *head, const char *name)
{
    struct rbh_node_info *new_node = malloc(sizeof(*new_node));

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

static void
capabilities_translate(const struct rbh_backend_plugin *plugin)
{
    const uint8_t capabilities = plugin->capabilities;

    printf("Capabilities of %s:\n",plugin->plugin.name);
    if (capabilities & RBH_FILTER_OPS)
        printf("- filter: rbh-find [source]\n");
    if (capabilities & RBH_SYNC_OPS)
        printf("- synchronisation: rbh-sync [source]\n");
    if (capabilities & RBH_UPDATE_OPS)
        printf("- update: rbh-sync [target]\n");
    if (capabilities & RBH_BRANCH_OPS)
        printf("- branch: rbh-sync [source for partial processing]\n");
}

static void
info_translate(const struct rbh_backend_plugin *plugin)
{
    const uint8_t info = plugin->info;

    printf("Info of %s:\n", plugin->plugin.name);
    if (info & RBH_INFO_SIZE)
        printf("- size:  -s   get the size of the given backend\n");
}

static int
help()
{
    const char *message =
        "Usage:"
        "  %s <URI> -uri_arguments   Show info about the given URI\n"
        "URI arguments:\n"
        "  -s --size                 Show the size of a given backend\n\n"
        "Arguments:\n"
        "Usage:"
        "  %s -arguments General informations about rbh-info command\n"
        "  -h --help                 Show this message and exit\n"
        "  -l --list                 Show the list of installed backends\n\n"
        "Backends capabilities list:\n"
        "Usage:"
        "  rbh-info <backend name>   Show capabilities of the given backend\n"
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

    if (dp == NULL)
        return 0;

    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, prefix))
            if (is_name_in_list(head, entry->d_name) == false)
                add_list(head, entry->d_name);
    }

    closedir(dp);
    return 0;
}

static int
print_backend_list(struct rbh_list_node *head)
{
    struct rbh_node_info *node;
    struct rbh_node_info *tmp;

    printf("List of installed backends:\n");

    rbh_list_foreach_safe(head, node, tmp, list) {
        char *backend_name = node->name + strlen(LIB_RBH_PREFIX);
        char *suffix_backend_name = strchr(backend_name, '.');

        if (suffix_backend_name) {
            *suffix_backend_name = '\0';
            if (rbh_backend_plugin_import(backend_name))
                printf("- %s\n", backend_name);
        }

        free(node->name);
        free(node);
    }
    return 0;
}

static int
check_ld_library_path(const char *pattern, struct rbh_list_node *head)
{
    const char *env = getenv("LD_LIBRARY_PATH");
    char *ld_library_path;
    char *path;

    if (env == NULL)
        return 0;

    ld_library_path = strdup(env);
    if (ld_library_path == NULL) {
        perror("strdup");
        return 0;
    }

    path = strtok(ld_library_path, ":");
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
    struct rbh_list_node *head = malloc(sizeof(struct rbh_list_node));

    if (head == NULL) {
        perror("malloc");
        return 1;
    }

    rbh_list_init(head);

    if (check_ld_library_path(LIB_RBH_PREFIX, head) && !rbh_list_empty(head)) {
        print_backend_list(head);
        free(head);
        return 0;
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
    return 0;
}

static int
backend_size(const struct rbh_backend_plugin *plugin, enum rbh_info size)
{
    const uint8_t info = plugin->info;

    if (info & RBH_INFO_SIZE) {
        rbh_backend_get_info(from, info);
    } else {
        printf("Size unavailable for %s backend, Please refer to the helper\n",
               from->name);
        return 0;
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
        {
            .name = "size",
            .val = 's',
        },
        {
            .name = "first_sync",
            .val = 'f',
        },
        {
            .name = "last_sync",
            .val = 'y',
        },
        {}
    };
    const struct rbh_backend_plugin *plugin;
    int flags = 0;
    int option;

    if (argc == 1){
        fprintf(stderr, "No backend name given, Please give a backend name\n");
        help();
        return EINVAL;
    }

    while ((option = getopt_long(argc, argv, "hls", LONG_OPTIONS,
                                 NULL)) != -1) {
        switch (option) {
        case 'h':
            help();
            return 0;
        case 'l':
            rbh_backend_list();
            return 0;
        case 's':
            flags |= RBH_SIZE_FLAG;
            break;
        default :
            fprintf(stderr, "Unrecognized option\n");
            help();
            return EINVAL;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc < 1)
        error(EX_USAGE, 0, "not enough arguments\n");
    if (argc > 1)
        error(EX_USAGE, 0, "unexpected argument: %s\n", argv[1]);

    if ((plugin = rbh_backend_plugin_import(argv[0]))) {
        capabilities_translate(plugin);
        return 0;
    }

    from = rbh_backend_from_uri(argv[0]);
    plugin = rbh_backend_plugin_import(from->name);

    if (plugin == NULL) {
        fprintf(stderr, "This backend does not exist\n");
        return EINVAL;
    }
    if (flags & RBH_SIZE_FLAG) {
        backend_size(plugin, RBH_INFO_SIZE);
    }
    if (!flags) {
        info_translate(plugin);
        return 0;
    }

    rbh_config_free();

    return EXIT_SUCCESS;
}
