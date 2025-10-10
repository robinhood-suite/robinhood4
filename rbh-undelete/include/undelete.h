/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_UNDELETE_UNDELETE_H
#define RBH_UNDELETE_UNDELETE_H

#include "robinhood/backend.h"

struct undelete_context {
    /**
     * The metadata source from which to fetch information about the entry to
     * undelete or list.
     */
    struct rbh_backend *source;

    /** The target entry to undelete */
    struct rbh_backend *target;

    /**
     * Path of the target, both in absolute and relative to the mountpoint form.
     *
     * XXX: to be fully generic, these should be encapsulated in a structure
     * dependant on the backend requested in the target URI, or as functions
     * in the backend itself. For simplicity's sake, since the tool is only
     * for Lustre currently, this isn't the case.
     */
    char *absolute_target_path;
    char *relative_target_path;
    char *mountpoint;
};

/**
 * Retrieve the mountpoint either from the current path and source backend, or
 * solely from the source backend if that fails.
 *
 * @param context    context of the command's execution
 *
 * @return           the mountpoint of the entry to undelete, or NULL on error
 *
 * Returned mountpoint should be freed by the caller.
 */
char *
get_mountpoint(struct undelete_context *context);

/**
 * Retrieve the absolute and relative paths of the target to undelete, and set
 * it in the command's context.
 *
 * @param target     the target path
 * @param context    context of the command's execution
 *
 * @return           0 on success, -1 on error with errno set
 */
int
set_targets(const char *target, struct undelete_context *context);

/**
 * List all entries under the relative target path in \p context that are
 * eligible to be undeleted.
 *
 * @param context    context of the command's execution
 *
 * @return           0 on success, -1 on error with errno set
 */
int
list_deleted_entries(struct undelete_context *context);

/**
 * Undelete an entry with the absolute path specified in \p context.
 *
 * If \p output is not NULL, undelete the target to that location.
 *
 * @param context    context of the command's execution
 * @param output     optional location where to undelete the target, can be NULL
 *
 * @return           0 on success, -1 on error with errno set
 */
int
undelete(struct undelete_context *context, const char *output);

#endif
