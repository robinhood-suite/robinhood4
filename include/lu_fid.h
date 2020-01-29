/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#include <stdint.h>

/* This file provides lustre-specific data structures for compatibility
 * purposes.
 *
 * It allows RobinHood not to depend on Lustre.
 */

struct lu_fid {
    uint64_t f_seq;
    uint32_t f_oid;
    uint32_t f_ver;
};

/**
 * Parse a string into a struct lu_fid
 *
 * @param string    a string representing a FID
 * @param fid       a pointer to a struct lu_fid where to store the information
 *                  parsed from \p string
 * @param endptr    the address of a pointer that is updated on success to point
 *                  after the last parsed character of \p string
 *
 * @return          0 on success, -1 on error and errno is appropriately set
 *
 * @error EINVAL    \p string is syntactically not a valid FID
 * @error ERANGE    \p string is numerically not a valid FID
 */
int
lu_fid_init_from_string(const char *string, struct lu_fid *fid, char **endptr);
