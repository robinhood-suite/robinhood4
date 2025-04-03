/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef RBH_FSEVENT_UTILS_H
#define RBH_FSEVENT_UTILS_H

/**
 * @file
 *
 * Utility functions to manage struct rbh_fsevent
 *
 * An RBH_FET_XATTR event can contain several elements:
 * 1. an extended attribute to store in the backend:
 *   xattrs:
 *       fid: <binary>
 * 2. a partial extended attribute that the enricher should fetch:
 *   xattrs:
 *       rbh-fsevents:
 *           - "user.test"
 * 3. a specific attribute to enrich that is specific to the source. For example,
 *   with Lustre:
 *   xattrs:
 *       rbh-fsevents:
 *           - "lustre"
 *
 * The following prototypes give tools to more easily manipulate these different
 * types of FS events. The first type will be refered to as "xattr", the second
 * one as "partial xattr" and the last one as "enrich elements".
 */

#include <robinhood.h>

const struct rbh_value *
rbh_map_find(const struct rbh_value_map *map, const char *key);

const struct rbh_value_map *
rbh_fsevent_find_fsevents_map(const struct rbh_fsevent *fsevent);

const struct rbh_value *
rbh_fsevent_find_partial_xattr(const struct rbh_fsevent *fsevent,
                               const char *key);

const struct rbh_value_pair *
rbh_fsevent_find_enrich_element(const struct rbh_fsevent *fsevent,
                                const char *key);

const struct rbh_value_pair *
rbh_fsevent_find_xattr(const struct rbh_fsevent *fsevent,
                       const char *key);

int
rbh_fsevent_deep_copy(struct rbh_fsevent *dst,
                      const struct rbh_fsevent *src,
                      struct rbh_sstack *stack);

int
rbh_value_deep_copy(struct rbh_value *dest, const struct rbh_value *src,
                    struct rbh_sstack *stack);

#endif
