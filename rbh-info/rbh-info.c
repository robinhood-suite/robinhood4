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
#define WIDTH 32

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

struct rbh_info_fields {
    char *field_name;
    void (*value_function)(const struct rbh_value *value);
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

    if (!info) {
        printf("Currently no info available for %s backend\n",
               plugin->plugin.name);
        return;
    }

    printf("Available info for backend '%s': \n", plugin->plugin.name);
    if (info & RBH_INFO_AVG_OBJ_SIZE)
        printf("- a: give the average size of objects inside entries collection\n");

    if (info & RBH_INFO_BACKEND_SOURCE)
        printf("- b: give the backend sources of the backend\n");

    if (info & RBH_INFO_COUNT)
        printf("- c: retrieve the amount of document inside entries collection\n");

    if (info & RBH_INFO_SIZE)
        printf("- s: size of entries collection\n");

    if (info & RBH_INFO_FIRST_SYNC)
        printf("- f: info about the first rbh-sync done\n");

    if (info & RBH_INFO_SIZE)
        printf("- s: size of entries collection\n");

    if (info & RBH_INFO_LAST_SYNC)
        printf("- y: info about the last rbh-sync done\n");
}

static int
help()
{
    const char *message =
        "Usage: %s [-hl]\n"
        "\n"
        "Show information about plugins\n"
        "\n"
        "Optional arguments:\n"
        "    -h, --help             Show this message and exit\n"
        "    -l, --list             Show the list of installed plugins\n"
        "\n"
        "Usage: %s [-h] PLUGIN\n"
        "\n"
        "Show capabilities of the given plugin\n"
        "\n"
        "Positional argument:\n"
        "    PLUGIN                 a robinhood plugin\n"
        "\n"
        "Optional arguments:\n"
        "    -h, --help             Show this message and exit\n"
        "\n"
        "Plugins capabilities list:\n"
        "- filter: The ability to read the data after filtering it according to"
        " different criteria\n"
        "- synchronisation: The ability to read the data\n"
        "- update: The ability to update information or metadata of files in"
        " the backend\n"
        "- branch: The ability to read data over a subsection of a backend\n"
        "\n"
        "Usage: %s [PRE_URI_OPTIONS] URI [POST_URI_OPTIONS]\n"
        "\n"
        "Show information about the given URI\n"
        "\n"
        "Positional arguments:\n"
        "    URI                    a robinhood URI\n"
        "\n"
        "Pre URI optional arguments:\n"
        "    -c, --config          The configuration file to use\n"
        "    -h, --help            Show this message and exit\n"
        "\n"
        "Post URI optional arguments:\n"
        "    -a, --avg-obj-size     Show the average size of objects inside\n"
        "                           a given backend\n"
        "    -b, --backend-source   Show the backend used as source for past\n"
        "                           rbh-syncs\n"
        "    -c, --count            Show the amount of document inside a\n"
        "                           given backend\n"
        "    -f, --first-sync       Show infos about the first rbh-sync done\n"
        "    -y, --last-sync        Show infos about the last rbh-sync done\n"
        "    -s, --size             Show the size of entries collection\n"
        "\n"
        "A robinhood URI is built as follows:\n"
        "    "RBH_SCHEME":BACKEND:FSNAME[#{PATH|ID}]\n\n";
            return printf(message, program_invocation_short_name,
                          program_invocation_short_name,
                          program_invocation_short_name);
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

    printf("List of installed plugins:\n");

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
        "/lib64",
        "/usr/lib64",
    };
    int len_library = sizeof(library_dirs) / sizeof(library_dirs[0]);
    struct rbh_list_node *head = malloc(sizeof(struct rbh_list_node));

    if (head == NULL) {
        perror("malloc");
        return 1;
    }

    rbh_list_init(head);

    if (check_ld_library_path(LIB_RBH_PREFIX, head) && !rbh_list_empty(head)) {
        // FIXME We don't go through library_dirs if LD_LIBRARY_PATH is set
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

static void
_get_collection_avg_obj_size(const struct rbh_value *value)
{
    char _buffer[32];
    char *buffer;

    buffer = _buffer;

    size_printer(buffer, sizeof(_buffer), value->int32);

    printf("%s\n", buffer);
}

static void
_get_collection_size(const struct rbh_value *value)
{
    char _buffer[32];
    char *buffer;

    buffer = _buffer;

    size_printer(buffer, sizeof(_buffer), value->int32);

    printf("%s\n", buffer);
}

static void
_get_collection_count(const struct rbh_value *value)
{
    printf("%ld\n", value->int64);
}

static void
_get_backend_source(const struct rbh_value *value)
{
    assert(value->type == RBH_VT_SEQUENCE);

    for (size_t i = 0; i < value->sequence.count; i++) {
        const struct rbh_value_map *submap = &value->sequence.values[i].map;
        size_t type_index = -1;
        bool is_plugin;
        size_t j;

        for (j = 0; j < submap->count; j++) {
            if (strcmp(submap->pairs[j].key, "type") == 0) {
                type_index = j;
                break;
            }
        }

        assert(j != -1);
        assert(submap->pairs[type_index].value->type == RBH_VT_STRING);
        is_plugin = (strcmp(submap->pairs[type_index].value->string,
                            "plugin") == 0);

        for (j = 0; j < submap->count; j++)
            if ((is_plugin && strcmp(submap->pairs[j].key, "plugin") == 0) ||
                (!is_plugin && strcmp(submap->pairs[j].key, "extension") == 0))
                printf("%s\n", submap->pairs[j].value->string);
    }
}

static void
_get_collection_sync(const struct rbh_value *value)
{
    assert(value->type == RBH_VT_MAP);

    const struct rbh_value_map metadata_map = value->map;
    time_t time;

    for (size_t i = 0 ; i < metadata_map.count ; i++) {
        const struct rbh_value_pair *pair = &metadata_map.pairs[i];
        if (strcmp(pair->key, "sync_debut") == 0) {
            time = (time_t)pair->value->int64;
            printf("%-*s %s\n", WIDTH, "Start of the sync:",
                   time_from_timestamp(&time));
        }

        if (strcmp(pair->key, "sync_duration") == 0) {
            char _buffer[32];
            size_t bufsize;
            char *buffer;

            buffer = _buffer;
            bufsize = sizeof(_buffer);

            difftime_printer(buffer, bufsize, pair->value->int64);

            printf("%-*s %s\n", WIDTH, "Duration of the sync:",
                   buffer);
        }

        if (strcmp(pair->key, "sync_end") == 0) {
            time = (time_t)pair->value->int64;
            printf("%-*s %s\n", WIDTH, "End of the sync:",
                   time_from_timestamp(&time));
        }

        if (strcmp(pair->key, "mountpoint") == 0)
            printf("%-*s %s\n", WIDTH, "Mountpoint used for the sync:",
                   pair->value->string);

        if (strcmp(pair->key, "command_line") == 0)
            printf("%-*s %s\n", WIDTH, "Command used for the sync:",
                   pair->value->string);

        if (strcmp(pair->key, "converted_entries") == 0)
            printf("%-*s %ld\n", WIDTH, "Amount of entries converted:",
                   pair->value->int64);

        if (strcmp(pair->key, "skipped_entries") == 0)
            printf("%-*s %ld\n", WIDTH, "Amount of entries skipped:",
                   pair->value->int64);

        if (strcmp(pair->key, "total_entries_seen") == 0)
            printf("%-*s %ld\n", WIDTH,
                   "Total entries seen by the sync:",
                   pair->value->int64);
    }
}

static struct rbh_info_fields INFO_FIELDS[] = {
    { "average_object_size", _get_collection_avg_obj_size },
    { "count", _get_collection_count },
    { "first_sync", _get_collection_sync },
    { "last_sync", _get_collection_sync },
    { "size", _get_collection_size },
    { "backend_source", _get_backend_source},
};

void
print_info_fields(int flags)
{
    struct rbh_value_map *info_map = rbh_backend_get_info(from, flags);
    size_t field_count = sizeof(INFO_FIELDS) / sizeof(INFO_FIELDS[0]);

    if (info_map == NULL) {
        printf("Failed to retrieve backend info.\n");
        return;
    }

    for (size_t i = 0 ; i < info_map->count ; i++) {
        const struct rbh_value_pair *pair = &info_map->pairs[i];

        for (size_t j = 0 ; j < field_count ; j++) {
            if (strcmp(pair->key, INFO_FIELDS[j].field_name) == 0) {
                INFO_FIELDS[j].value_function(pair->value);
            }
        }
    }
}

static void
apply_command_options(int argc, char *argv[])
{
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            help();
            exit(0);
        }

        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 >= argc)
                error(EXIT_FAILURE, EINVAL,
                      "missing configuration file value");

            rbh_config_load_from_path(argv[i + 1]);
        }

        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
            rbh_backend_list();
            exit(0);
        }
    }
}

int
main(int _argc, char **_argv)
{
    const struct option LONG_OPTIONS[] = {
        {
            .name = "avg-obj-size",
            .val = 'a',
        },
        {
            .name = "backend-source",
            .val = 'b',
        },
        {
            .name = "count",
            .val = 'c',
        },
        {
            .name = "first-sync",
            .val = 'f',
        },
        {
            .name = "help",
            .val = 'h'
        },
        {
            .name = "last-sync",
            .val = 'y',
        },
        {
            .name = "list",
            .val = 'l'
        },
        {
            .name = "size",
            .val = 's',
        },
        {}
    };
    const struct rbh_backend_plugin *plugin;
    int flags = 0, option;
    int nb_cli_args;
    char **argv;
    int argc;

    nb_cli_args = rbh_count_args_before_uri(_argc, _argv);
    apply_command_options(nb_cli_args, _argv);

    if (nb_cli_args == _argc) {
        argc = _argc - 1;
        argv = &_argv[1];
    } else {
        argc = _argc - nb_cli_args;
        argv = &_argv[nb_cli_args];
    }

    while ((option = getopt_long(argc, argv, "abcfhlsy", LONG_OPTIONS,
                                 NULL)) != -1) {
        switch (option) {
        case 'a':
            flags |= RBH_INFO_AVG_OBJ_SIZE;
            break;
        case 'b':
            flags |= RBH_INFO_BACKEND_SOURCE;
            break;
        case 'c':
            flags |= RBH_INFO_COUNT;
            break;
        case 'f':
            flags |= RBH_INFO_FIRST_SYNC;
            break;
        case 'h':
            help();
            return 0;
        case 'l':
            rbh_backend_list();
            return 0;
        case 's':
            flags |= RBH_INFO_SIZE;
            break;
        case 'y':
            flags |= RBH_INFO_LAST_SYNC;
            break;
        default :
            fprintf(stderr, "Unrecognized option\n");
            help();
            return EINVAL;
        }
    }

    if (argc < 1)
        error(EX_USAGE, 0, "not enough arguments\n");

    if (!rbh_is_uri(argv[0])) {
        plugin = rbh_backend_plugin_import(argv[0]);
        if (plugin == NULL) {
            fprintf(stderr, "This backend does not exist\n");
            return EINVAL;
        }
        capabilities_translate(plugin);
        return 0;
    }

    from = rbh_backend_from_uri(argv[0], true);
    plugin = rbh_backend_plugin_import(from->name);

    if (plugin == NULL) {
        fprintf(stderr, "This plugin does not exist\n");
        return EINVAL;
    }
    if (flags)
        print_info_fields(flags);
    if (!flags)
        info_translate(plugin);

    return EXIT_SUCCESS;
}
