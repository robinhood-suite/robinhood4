/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_PLUGIN_H
#define RBH_PLUGIN_H

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <robinhood/filter.h>
#include <robinhood/fsentry.h>

#include <robinhood/plugins/common_ops.h>

struct rbh_plugin {
    const char *name;
    uint64_t version;
};

struct rbh_plugin_extension {
    const char *super;
    const char *name;
    uint64_t version;
    uint64_t min_version;
    uint64_t max_version;
    const struct rbh_pe_common_operations *common_ops;
};

/**
 * Import a plugin
 *
 * @param name      the name of the plugin (from which the dynamic library name
 *                  is inferred)
 * @param symbol    the name of the actual symbol to dlsym() from the plugin's
 *                  dynamic library
 *
 * @return          the address where \p symbol is loaded into memory on
 *                  success, NULL on error and dlerror() can be used to
 *                  establish a diagnostic.
 */
void *
rbh_plugin_import(const char *name, const char *symbol);

/*----------------------------------------------------------------------------*
 |                          Robinhood Plugin Version                          |
 *----------------------------------------------------------------------------*/

/* Version numbers are stored in 64 bits long unsigned integers as follows:
 *
 * 0                          27                         54        64
 * -----------------------------------------------------------------
 * |         revision         |           minor          |  major  |
 * -----------------------------------------------------------------
 *
 */

#define RPV_MAJOR_SHIFT 54
#define RPV_MINOR_SHIFT 27

#define RPV(major, minor, revision) \
    ((UINT64_C(major) << RPV_MAJOR_SHIFT) \
   + (UINT64_C(minor) << RPV_MINOR_SHIFT) \
   + UINT64_C(revision))

#define RPV_MAJOR_MASK      0xffc0000000000000
#define RPV_MINOR_MASK      0x003ffffff8000000
#define RPV_REVISION_MASK   0x0000000007ffffff

#define RPV_MAJOR(version) ((version) >> RPV_MAJOR_SHIFT)
#define RPV_MINOR(version) (((version) & RPV_MINOR_MASK) >> RPV_MINOR_SHIFT)
#define RPV_REVISION(version) ((version) & RPV_REVISION_MASK)

/**
 * Binary capabilities indicator to identify the presence of options: filter,
 * update and branch in the backends.
 */
#define RBH_FILTER_OPS 0b1000
#define RBH_UPDATE_OPS 0b0100
#define RBH_BRANCH_OPS 0b0010
#define RBH_SYNC_OPS 0b0001
#define RBH_EMPTY_OPS 0b0000

#endif
