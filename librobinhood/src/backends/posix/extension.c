/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <robinhood/backends/posix_extension.h>
#include <robinhood/statx.h>
#include <robinhood/open.h>

#include <fcntl.h>

int
rbh_posix_enrich_open_by_id(struct rbh_posix_enrich_ctx *ctx,
                            int parent_fd,
                            const struct rbh_id *id)
{
    int fd;

    if (ctx->einfo.fd > 0)
        return 0;

    fd = open_by_id_generic(parent_fd, id);
    if (fd < 0 && errno == ELOOP)
        /* If the file to open is a symlink, reopen it with O_PATH set */
        fd = open_by_id_opath(parent_fd, id);
    if (fd < 0)
        return -1;

    ctx->einfo.fd = fd;

    return 0;
}

int
rbh_posix_enrich_statx(struct rbh_posix_enrich_ctx *ctx, int flags,
                       unsigned int mask, struct rbh_statx *restrict statxbuf)
{
    int rc;

    if (ctx->einfo.statx)
        return 0;

    /* FIXME: We should really use AT_RBH_STATX_FORCE_SYNC here */
    /* Make sure to retrieve the mode as well as this will be used by the
     * Lustre enricher as well.
     */
    rc = rbh_statx(ctx->einfo.fd, "", flags, mask | RBH_STATX_MODE, statxbuf);

    ctx->einfo.statx = statxbuf;

    return rc;
}
