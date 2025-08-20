/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_LUSTRE
#include <lustre/lustre_user.h>
#endif

#include "hash.h"

static size_t
dbj2(const char *buf, size_t size)
{
    size_t hash = 5381;

    for (size_t i = 0; i < size; i++)
        hash = ((hash << 5) + hash) + buf[i];

    return hash;
}

size_t
hash_id(const struct rbh_id *id)
{
    return dbj2(id->data, id->size);
}

// Taken from robinhood v3's implementation
// Murmur3 uint64 finalizer from:
// https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
static inline size_t
hash64(size_t k)
{
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdLLU;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53LLU;
    k ^= k >> 33;
    return k;
}

size_t
hash_lu_id(const struct rbh_id *id)
{
#ifdef HAVE_LUSTRE
    const struct lu_fid *fid = rbh_lu_fid_from_id(id);

    return hash64(fid->f_seq ^ fid->f_oid);
#else
    return hash_id(id);
#endif

}
