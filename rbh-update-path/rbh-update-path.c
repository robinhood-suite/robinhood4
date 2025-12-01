/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <error.h>
#include <getopt.h>
#include <sysexits.h>

#include <robinhood.h>

#ifndef RBH_ITER_CHUNK_SIZE
# define RBH_ITER_CHUNK_SIZE (1 << 12)
#endif

static struct rbh_backend *backend;

static void __attribute__((destructor))
destroy_backend(void)
{
    const char *name;

    if (backend) {
        name = backend->name;
        rbh_backend_destroy(backend);
        rbh_backend_plugin_destroy(name);
    }
}

static struct rbh_mut_iterator *
_get_entries(struct rbh_filter *filter)
{
    const struct rbh_filter_projection proj = {
        .fsentry_mask = RBH_FP_ID | RBH_FP_PARENT_ID | RBH_FP_NAME |
                        RBH_FP_NAMESPACE_XATTRS | RBH_FP_STATX,
        .statx_mask = RBH_STATX_TYPE,
    };
    const struct rbh_filter_options option = {0};
    const struct rbh_filter_output output = {
        .type = RBH_FOT_PROJECTION,
        .projection = proj,
    };
    struct rbh_mut_iterator *fsentries;

    fsentries = rbh_backend_filter(backend, filter, &option, &output, NULL);
    if (fsentries == NULL) {
        if (errno == RBH_BACKEND_ERROR)
            error(EXIT_FAILURE, 0, "%s", rbh_backend_error);
        else
            error(EXIT_FAILURE, errno,
                  "failed to execute filter on backend '%s'", backend->name);
    }

    free(filter);

    return fsentries;
}

static struct rbh_mut_iterator *
get_entry_children(struct rbh_fsentry *entry)
{
    const struct rbh_filter_field *field;
    struct rbh_filter *filter;

    field = str2filter_field("parent-id");
    filter = rbh_filter_compare_binary_new(RBH_FOP_EQUAL, field,
                                           entry->id.data, entry->id.size);
    if (filter == NULL)
        error(EXIT_FAILURE, errno, "failed to create filter");

    return _get_entries(filter);
}

static struct rbh_mut_iterator *
get_entry_without_path()
{
    const struct rbh_filter_field *field = str2filter_field("ns-xattrs.path");
    struct rbh_filter *filter_path;
    struct rbh_filter *filter;

    filter_path = rbh_filter_exists_new(field);
    if (filter_path == NULL)
        error(EXIT_FAILURE, errno, "failed to create path filter");

    filter = rbh_filter_not(filter_path);

    return _get_entries(filter);
}

static struct rbh_fsentry *
get_entry_parent(struct rbh_fsentry *entry)
{
    const struct rbh_filter_projection proj = {
        .fsentry_mask = RBH_FP_ID | RBH_FP_PARENT_ID | RBH_FP_NAME |
                        RBH_FP_NAMESPACE_XATTRS,
        .statx_mask = 0,
    };
    const struct rbh_filter_field *field;
    struct rbh_fsentry *parent;
    struct rbh_filter *filter;

    field = str2filter_field("id");
    filter = rbh_filter_compare_binary_new(RBH_FOP_EQUAL, field,
                                           entry->parent_id.data,
                                           entry->parent_id.size);
    if (filter == NULL)
        error(EXIT_FAILURE, errno, "failed to create filter");

    parent = rbh_backend_filter_one(backend, filter, &proj);

    free(filter);

    return parent;
}

static struct rbh_fsevent *
generate_fsevent_ns_xattrs(struct rbh_fsentry *entry, struct rbh_value *value)
{
    struct rbh_value_map xattrs;
    struct rbh_fsevent *fsevent;
    struct rbh_value_pair pair;

    pair.key = "path";
    pair.value = value;

    xattrs.pairs = &pair;
    xattrs.count = 1;

    fsevent = rbh_fsevent_ns_xattr_new(&entry->id, &xattrs, &entry->parent_id,
                                       entry->name);

    if (fsevent == NULL)
        error(EXIT_FAILURE, errno, "failed to generate fsevent");

    return fsevent;
}

static struct rbh_fsevent *
generate_fsevent_update_path(struct rbh_fsentry *entry,
                             struct rbh_fsentry *parent,
                             const struct rbh_value *value_path)
{
    struct rbh_fsevent *fsevent;
    struct rbh_value value;
    char *format;
    char *path;

    if (strcmp(value_path->string, "/") == 0)
        format = "%s%s";
    else
        format = "%s/%s";

    if (asprintf(&path, format, value_path->string, entry->name) == -1)
        error(EXIT_FAILURE, errno, "failed to create the path");

    value.type = RBH_VT_STRING;
    value.string = path;

    fsevent = generate_fsevent_ns_xattrs(entry, &value);

    free(path);

    return fsevent;
}

struct rbh_node_fsevent {
    struct rbh_fsevent fsevent;
    struct rbh_list_node link;
};

static int
add_fsevents_child(struct rbh_list_node *list, struct rbh_fsevent *fsevent,
                   struct rbh_sstack *stack)
{
    struct rbh_node_fsevent *node = xmalloc(sizeof(*node));

    rbh_fsevent_deep_copy(&node->fsevent, fsevent, stack);

    rbh_list_add_tail(list, &node->link);

    return 0;
}

static void
free_fsevents_list(struct rbh_list_node *list)
{
    struct rbh_node_fsevent *elem, *tmp;

    rbh_list_foreach_safe(list, elem, tmp, link) {
        free(elem);
    }

    free(list);
}

/* Remove the ns.xattrs.path on all the children of an entry.
 *
 * @param entry
 *
 * @return true if we have remove the ns.xattrs.path of at least one child,
 *         false otherwise.
 */
static bool
remove_children_path(struct rbh_fsentry *entry)
{
    struct rbh_mut_iterator * children;
    struct rbh_iterator *update_iter;
    struct rbh_mut_iterator *chunks;
    struct rbh_list_node *fsevents;
    struct rbh_fsevent *fsevent;
    struct rbh_sstack *stack;

    fsevents = xmalloc(sizeof(*fsevents));
    rbh_list_init(fsevents);

    children = get_entry_children(entry);

    stack = rbh_sstack_new(1 << 10);

    while (true) {
        struct rbh_fsentry *child = rbh_mut_iter_next(children);

        if (child == NULL) {
            if (errno == ENODATA)
                break;

            if (errno == RBH_BACKEND_ERROR)
                error(EXIT_FAILURE, 0, "%s", rbh_backend_error);
            else
                error(EXIT_FAILURE, errno, "failed to retrieve child");
        }

        fsevent = generate_fsevent_ns_xattrs(child, NULL);
        add_fsevents_child(fsevents, fsevent, stack);
        free(fsevent);
        free(child);
    }

    rbh_mut_iter_destroy(children);

    if (rbh_list_empty(fsevents)) {
        rbh_sstack_destroy(stack);
        free(fsevents);
        return false;
    }

    update_iter = rbh_iter_list(fsevents,
                                offsetof(struct rbh_node_fsevent, link),
                                free_fsevents_list);

    /* XXX: the mongo backend tries to process all the fsevents at once in a
     *      single bulk operation, but a bulk operation is limited in size.
     *
     * Splitting `fsevents' into fixed-size sub-iterators solves this.
     */
    chunks = rbh_iter_chunkify(update_iter, RBH_ITER_CHUNK_SIZE);
    if (chunks == NULL)
        error(EXIT_FAILURE, errno, "rbh_mut_iter_chunkify");

    do {
        struct rbh_iterator *chunk = rbh_mut_iter_next(chunks);
        int save_errno;
        ssize_t count;

        if (chunk == NULL) {
            if (errno == ENODATA)
                break;
            error(EXIT_FAILURE, errno, "while chunkifying fsevents");
        }

        count = rbh_backend_update(backend, chunk);
        save_errno = errno;
        rbh_iter_destroy(chunk);
        if (count < 0) {
            errno = save_errno;
            error(EXIT_FAILURE, errno, "rbh_backend_update");
        }
    } while (true);

    rbh_mut_iter_destroy(chunks);
    rbh_sstack_destroy(stack);

    return true;
}

static bool
update_path()
{
    struct rbh_mut_iterator *fsentries;
    const struct rbh_value *value_path;
    bool has_update_children = false;
    struct rbh_iterator *update_iter;
    struct rbh_fsevent *fsevent;
    int rc;

    fsentries = get_entry_without_path();

    while (true) {
        struct rbh_fsentry *parent;
        struct rbh_fsentry *entry;

        entry = rbh_mut_iter_next(fsentries);

        /* TODO: store all the directories to avoid querying the backend */
        if (entry == NULL) {
            if (errno == ENODATA)
                break;

            if (errno == RBH_BACKEND_ERROR)
                error(EXIT_FAILURE, 0, "%s", rbh_backend_error);
            else
                error(EXIT_FAILURE, errno, "failed to retrieve entry");
        }

        /* If it's not a directory, no need to update it's children */
        if (!S_ISDIR(entry->statx->stx_mode))
            goto update_path;

        has_update_children = remove_children_path(entry);

update_path:
        /* Update entry path */
        parent = get_entry_parent(entry);
        if (entry == NULL) {
            /* Skip this entry if it doesn't have a parent, will be updated
             * later
             */
            if (errno == ENODATA)
                continue;

            if (errno == RBH_BACKEND_ERROR)
                error(EXIT_FAILURE, 0, "%s", rbh_backend_error);
            else
                error(EXIT_FAILURE, errno,
                      "failed to get the parent of '%s'", entry->name);
        }

        value_path = rbh_fsentry_find_ns_xattr(parent, "path");
        /* Skip this entry if its parent doesn't have a path, will be updated
         * later
         */
        if (value_path == NULL)
            continue;

        fsevent = generate_fsevent_update_path(entry, parent, value_path);
        update_iter = rbh_iter_array(fsevent, sizeof(*fsevent), 1, NULL);

        rc = rbh_backend_update(backend, update_iter);
        if (rc == -1)
            error(EXIT_FAILURE, errno, "failed to update '%s'",
                  entry->name);

        rbh_iter_destroy(update_iter);
        free(fsevent);
        free(parent);
        free(entry);
    }

    rbh_mut_iter_destroy(fsentries);

    return has_update_children;
}

int
main(int argc, char *argv[])
{
    const struct option LONG_OPTIONS[] = {
        {
            .name = "config",
            .has_arg = required_argument,
            .val = 'c',
        },
        {}
    };
    int rc;
    char c;

    rc = rbh_config_from_args(argc - 1, argv + 1);
    if (rc)
        error(EXIT_FAILURE, errno, "failed to open configuration file");

    while ((c = getopt_long(argc, argv, "c:", LONG_OPTIONS, NULL)) != -1) {
        switch (c) {
        case 'c':
            /* already parsed */
            break;
        case '?':
        default:
            /* getopt_long() prints meaningful error messages itself */
            exit(EX_USAGE);
        }
    }

    argc -= optind;
    argv += optind;

    if (argc < 1)
        error(EX_USAGE, 0, "not enough arguments");
    if (argc > 1)
        error(EX_USAGE, 0, "unexpected argument: %s", argv[1]);

    backend = rbh_backend_from_uri(argv[0], false);

    while (true) {
        if (!update_path())
            break;
    }

    return EXIT_SUCCESS;
}
