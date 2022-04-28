#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include <lustre/lustreapi.h>

#include "robinhood/backends/posix.h"
#include "robinhood/backends/posix_internal.h"
#include "robinhood/backends/lustre.h"

static __thread struct rbh_sstack *_values;
static __thread bool is_dir;
static __thread bool is_reg;

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

static inline int
fill_uint32_pair(const char *key, uint32_t integer, struct rbh_value_pair *pair)
{
    const struct rbh_value uint32_value = {
        .type = RBH_VT_UINT32,
        .uint32 = integer,
    };

    return fill_pair(key, &uint32_value, pair);
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

/**
 * Record a file's hsm attributes (state and archive_id) in \p pairs
 *
 * @param fd        file descriptor to check
 * @param pairs     list of pairs to fill
 *
 * @return          number of filled \p pairs
 */
static int
xattrs_get_hsm(int fd, struct rbh_value_pair *pairs)
{
    struct hsm_user_state hus;
    int subcount = 0;
    int rc;

    if (is_dir)
        return 0;

    rc = llapi_hsm_state_get_fd(fd, &hus);
    if (rc) {
        errno = -rc;
        return -1;
    }

    rc = fill_uint32_pair("hsm_state", hus.hus_states, &pairs[subcount++]);
    if (rc)
        return -1;

    rc = fill_uint32_pair("hsm_archive_id", hus.hus_archive_id,
                          &pairs[subcount++]);
    if (rc)
        return -1;

    return subcount;
}

/**
 * Record a file's magic number and layout generation in \p pairs
 *
 * @param fd        file descriptor to check
 * @param pairs     list of pairs to fill
 *
 * @return          number of filled \p pairs
 */
static int
xattrs_get_magic_and_gen(int fd, struct rbh_value_pair *pairs)
{
    char lov_buf[XATTR_SIZE_MAX];
    ssize_t xattr_size;
    uint32_t magic = 0;
    int magic_str_len;
    int subcount = 0;
    uint32_t gen = 0;
    char *magic_str;
    int rc;

    xattr_size = fgetxattr(fd, XATTR_LUSTRE_LOV, lov_buf, sizeof(lov_buf));
    if (xattr_size < 0)
        return -1;

    magic = ((struct lov_user_md *) lov_buf)->lmm_magic;

    switch (magic) {
    case LOV_USER_MAGIC_V1:
        magic_str = "LOV_USER_MAGIC_V1";
        magic_str_len = 18;
        gen = ((struct lov_user_md_v1 *) lov_buf)->lmm_layout_gen;
        break;
    case LOV_USER_MAGIC_COMP_V1:
        magic_str = "LOV_USER_MAGIC_COMP_V1";
        magic_str_len = 23;
        gen = ((struct lov_comp_md_v1 *) lov_buf)->lcm_layout_gen;
        break;
    case LOV_USER_MAGIC_SEL:
        magic_str = "LOV_USER_MAGIC_SEL";
        magic_str_len = 19;
        gen = ((struct lov_comp_md_v1 *) lov_buf)->lcm_layout_gen;
        break;
    case LOV_USER_MAGIC_V3:
        magic_str = "LOV_USER_MAGIC_V3";
        magic_str_len = 18;
        gen = ((struct lov_user_md_v3 *) lov_buf)->lmm_layout_gen;
        break;
    case LOV_USER_MAGIC_SPECIFIC:
        magic_str = "LOV_USER_MAGIC_SPECIFIC";
        magic_str_len = 24;
        gen = ((struct lov_user_md_v3 *) lov_buf)->lmm_layout_gen;
        break;
    case LOV_USER_MAGIC_FOREIGN:
        magic_str = "LOV_USER_MAGIC_FOREIGN";
        magic_str_len = 23;
        gen = -1;
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    rc = fill_string_pair("magic", magic_str, magic_str_len,
                          &pairs[subcount++]);
    if (rc)
        return -1;

    rc = fill_uint32_pair("gen", gen, &pairs[subcount++]);
    if (rc)
        return -1;

    return subcount;
}

/**
 * Record a file's layout attributes (main flags, and magic number and layout
 * generation if it is a regular file) in \p pairs
 *
 * @param fd        file descriptor to check
 * @param pairs     list of pairs to fill
 *
 * @return          number of filled \p pairs
 */
static int
xattrs_get_layout(int fd, struct rbh_value_pair *pairs)
{
    struct llapi_layout *layout;
    int subcount = 0;
    uint32_t flags;
    int rc;

    layout = llapi_layout_get_by_fd(fd, 0);
    if (layout == NULL)
        return -1;

    rc = llapi_layout_flags_get(layout, &flags);
    if (rc)
        goto err;

    rc = fill_uint32_pair("flags", flags, &pairs[subcount++]);
    if (rc)
        goto err;

    if (is_reg) {
        rc = xattrs_get_magic_and_gen(fd, &pairs[subcount]);
        if (rc < 0)
            goto err;


        subcount += rc;
        rc = 0;
    }

err:
    llapi_layout_free(layout);
    return rc ? rc : subcount;
}

static ssize_t
lustre_ns_xattrs_callback(const int fd, const uint16_t mode,
                          struct rbh_value_pair *pairs,
                          struct rbh_sstack *values)
{
    int (*xattrs_funcs[])(int, struct rbh_value_pair *) = {
        xattrs_get_fid, xattrs_get_hsm, xattrs_get_layout
    };
    int count = 0;
    int subcount;

    is_dir = S_ISDIR(mode);
    is_reg = S_ISREG(mode);
    _values = values;

    for (int i = 0; i < sizeof(xattrs_funcs) / sizeof(xattrs_funcs[0]); ++i) {
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
