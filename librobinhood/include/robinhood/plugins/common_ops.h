/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_BACKEND_COMMON_OPS_H
#define ROBINHOOD_BACKEND_COMMON_OPS_H

/**
 * Define a list of operations common to both plugins and extensions.
 */
struct rbh_pe_common_operations {
    /**
     * Check the given token corresponds to a predicate or action.
     *
     * @param plugin    the plugin to check the token with
     * @param token     a string corresponding to the token to identify
     *
     * @return          RBH_TOKEN_PREDICATE if the token is a predicate
     *                  RBH_TOKEN_ACTION if the token is an action
     *                  RBH_TOKEN_UNKNOWN if the token is not valid
     *                  RBH_TOKEN_ERROR if an error occurs
     */
    enum rbh_parser_token (*check_valid_token)(const char *token);

    /**
     * Create a filter from the command line argument at \p index in \p argv
     *
     * @param argv           the list of arguments given to the command
     * @param argc           the number of strings in \p argv
     * @param index          the argument currently being parsed, should be
     *                       updated if necessary to skip optionnal values
     * @param need_prefetch  boolean value set by posix to indicate if a filter
     *                       will need to be completed
     *
     * @return               a pointer to newly allocated struct rbh_filter on
     *                       success, NULL on error and errno is set
     *                       appropriately
     */
    struct rbh_filter *(*build_filter)(const char **argv, int argc, int *index,
                                       bool *need_prefetch);

    /**
     * Fill information about an entry according to a given directive into a
     * buffer
     *
     * @param output         an array in which the information can be printed
     *                       according to \p directive.
     * @param max_length     available size which can be written in \p output
     * @param fsentry        fsentry whose info should be filled
     * @param directive      which information about \p fsentry to fill
     * @param backend        the backend to print (XXX: may not be necessary)
     *
     * @return               the number of characters written to \p output on
     *                       success
     *                       0 if the directive requested is unknown
     *                       -1 on error
     */
    int (*fill_entry_info)(char *output, int max_length,
                           const struct rbh_fsentry *fsentry,
                           const char *directive, const char *backend);

    /**
     * Delete an entry
     *
     * @param fsentry        the entry to delete
     *
     * @return               0 on success, -1 on error and errno is set
     */
    int (*delete_entry)(struct rbh_fsentry *fsentry);
};

/**
 * The following functions are wrappers around the functions defined in the
 * common_ops structure defined above. They will check the function requested
 * is defined and call them if needed, or return an error otherwise.
 *
 * They all use the same arguments as the common_ops functions, though they
 * add on argument, which is the common_ops structure to call.
 *
 * For each of their documentation, check the documentation of the common_ops
 * functions.
 */
static inline enum rbh_parser_token
rbh_pe_common_ops_check_valid_token(
    const struct rbh_pe_common_operations *common_ops,
    const char *token
)
{
    if (common_ops && common_ops->check_valid_token)
        return common_ops->check_valid_token(token);

    errno = ENOTSUP;
    return RBH_TOKEN_ERROR;
}

static inline struct rbh_filter *
rbh_pe_common_ops_build_filter(
    const struct rbh_pe_common_operations *common_ops,
    const char **argv, int argc, int *index, bool *need_prefetch
)
{
    if (common_ops && common_ops->build_filter)
        return common_ops->build_filter(argv, argc, index, need_prefetch);

    errno = ENOTSUP;
    return NULL;
}

static inline int
rbh_pe_common_ops_fill_entry_info(
    const struct rbh_pe_common_operations *common_ops,
    char *output, int max_length, const struct rbh_fsentry *fsentry,
    const char *directive, const char *backend
)
{
    if (common_ops && common_ops->fill_entry_info)
        return common_ops->fill_entry_info(output, max_length, fsentry,
                                            directive, backend);

    errno = ENOTSUP;
    return -1;
}

static inline int
rbh_pe_common_ops_delete_entry(
    const struct rbh_pe_common_operations *common_ops,
    struct rbh_fsentry *fsentry
)
{
    if (common_ops && common_ops->delete_entry)
        return common_ops->delete_entry(fsentry);

    errno = ENOTSUP;
    return -1;
}

#endif
