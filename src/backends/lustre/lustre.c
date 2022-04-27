#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <lustre/lustreapi.h>

#include "robinhood/backends/posix.h"
#include "robinhood/backends/posix_internal.h"
#include "robinhood/backends/lustre.h"

static __thread struct rbh_sstack *_values;

static inline int
fill_pair(const char *key, const struct rbh_value *value,
          struct rbh_value_pair *pair)
{
    pair->key = key;
    pair->value = rbh_sstack_push(_values, value, sizeof(*value));
    if (pair->value == NULL)
        return -1;

    return 0;
}

static inline int
fill_string_pair(const char *key, const char *str, const int len,
                 struct rbh_value_pair *pair)
{
    const struct rbh_value string_value = {
        .type = RBH_VT_STRING,
        .string = rbh_sstack_push(_values, str, len),
    };

    return fill_pair(key, &string_value, pair);
}

/**
 * Record a file's fid in \p pairs
 *
 * @param fd        file descriptor to check
 * @param pairs     list of pairs to fill
 *
 * @return          number of filled \p pairs
 */
static int
xattrs_get_fid(int fd, struct rbh_value_pair *pairs)
{
    char buf[FID_NOBRACE_LEN + 1];
    struct lu_fid fid;
    int rc;

    rc = llapi_fd2fid(fd, &fid);
    if (rc) {
        errno = -rc;
        return -1;
    }

    rc = snprintf(buf, FID_NOBRACE_LEN, DFID_NOBRACE, PFID(&fid));
    if (rc < 0) {
        errno = -EINVAL;
        return -1;
    }

    rc = fill_string_pair("fid", buf, FID_NOBRACE_LEN + 1, pairs);

    return rc ? : 1;
}

#define XATTRS_N_FUNC 1

static ssize_t
lustre_ns_xattrs_callback(const int fd, const uint16_t mode,
                          struct rbh_value_pair *pairs,
                          struct rbh_sstack *values)
{
    int (*xattrs_funcs[XATTRS_N_FUNC])(int, struct rbh_value_pair *) = {
        xattrs_get_fid
    };
    int count = 0;
    int subcount;

    _values = values;

    for (int i = 0; i < XATTRS_N_FUNC; ++i) {
        subcount = xattrs_funcs[i](fd, &pairs[count]);
        if (subcount == -1)
            return -1;

        count += subcount;
    }

    return count;
}

struct posix_iterator *
lustre_iterator_new(const char *root, const char *entry, int statx_sync_type)
{
    struct posix_iterator *lustre_iter;

    lustre_iter = posix_iterator_new(root, entry, statx_sync_type);
    if (lustre_iter == NULL)
        return NULL;

    lustre_iter->ns_xattrs_callback = lustre_ns_xattrs_callback;

    return lustre_iter;
}

struct rbh_backend *
rbh_lustre_backend_new(const char *path)
{
    struct posix_backend *lustre;

    lustre = (struct posix_backend *)rbh_posix_backend_new(path);
    if (lustre == NULL)
        return NULL;

    lustre->iter_new = lustre_iterator_new;
    lustre->backend.id = RBH_BI_LUSTRE;
    lustre->backend.name = RBH_LUSTRE_BACKEND_NAME;

    return &lustre->backend;
}
