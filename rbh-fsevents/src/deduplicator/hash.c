#include "hash.h"
#include <lustre/lustre_user.h>

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
hash_id(const struct rbh_id *id)
{
    const struct lu_fid *fid = rbh_lu_fid_from_id(id);

    // FIXME this is Lustre specific
    return hash64(fid->f_seq ^ fid->f_oid);
}
