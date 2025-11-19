/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <error.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <mpi.h>
#include <stddef.h>
#include <stdint.h>

#include "robinhood/backends/s3_mpi.h"
#include "robinhood/backends/s3_extension.h"
#include "robinhood/backends/s3.h"
#include "robinhood/config.h"
#include "robinhood/mpi_rc.h"
#include "robinhood/sstack.h"
#include "robinhood/statx.h"
#include "robinhood/utils.h"

#include "../../s3_internals.h"
#include "../../s3_wrapper.h"

/*----------------------------------------------------------------------------*
 |                                s3_iterator                                 |
 *----------------------------------------------------------------------------*/

static void
s3_mpi_iter_destroy(void *iterator)
{
    struct s3_iterator *s3_iter = iterator;

    s3_iter_destroy(s3_iter);
    rbh_mpi_dec_ref(rbh_mpi_finalize);
}

static const struct rbh_mut_iterator_operations S3_MPI_ITER_OPS = {
    .next = s3_iter_next,
    .destroy = s3_mpi_iter_destroy,
};

static const struct rbh_mut_iterator S3_MPI_ITER = {
    .ops = &S3_MPI_ITER_OPS,
};

int
get_sub_array_bounds(int array_size, int num_chunks, int chunk_index,
                     int *start, int *end)
{
    int base_chunk_size;
    int remainder;

    if (array_size < num_chunks)
        num_chunks = array_size;

    if (chunk_index >= array_size || array_size == 0)
        return -1;

    base_chunk_size = array_size / num_chunks;
    remainder = array_size % num_chunks;

    if (chunk_index < remainder)
        *start = chunk_index * base_chunk_size + chunk_index;
    else
        *start = chunk_index * base_chunk_size + remainder;

    if (chunk_index < remainder)
        *end = *start + base_chunk_size;
    else
        *end = *start + base_chunk_size - 1;

    return 0;
}

struct rbh_mut_iterator *
rbh_s3_mpi_iter_new()
{
    struct s3_iterator *s3_iter = NULL;
    size_t max_bucket_size = 64;
    int64_t tmp_length;
    char **tmp_list;
    int arr_start;
    int mpi_rank;
    int mpi_size;
    int sub_size;
    int arr_end;
    int rc;

    s3_iter = xmalloc(sizeof(*s3_iter));
    s3_iter->iterator = S3_MPI_ITER;

    rbh_mpi_inc_ref(rbh_mpi_initialize);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    s3_get_bucket_list(&tmp_length, &tmp_list);
    if (tmp_length == -1) {
        free(s3_iter);
        return NULL;
    }

    rc = get_sub_array_bounds(tmp_length, mpi_size, mpi_rank, &arr_start,
                              &arr_end);
    /* No buckets for this process, return an empty iterator */
    if (rc) {
        s3_iter->bkt_data.length = 0;
        s3_iter->obj_data.length = 0;
        s3_iter->values = NULL;
        return (struct rbh_mut_iterator *) s3_iter;
    }

    sub_size = arr_end - arr_start + 1;
    s3_iter->bkt_data.list = xmalloc(sub_size * sizeof(char *));
    s3_iter->bkt_data.length = sub_size;
    for (int i = 0; i < sub_size; i++) {
        s3_iter->bkt_data.list[i] = xmalloc(max_bucket_size * sizeof(char));
        strcpy(s3_iter->bkt_data.list[i], tmp_list[arr_start + i]);
    }

    s3_delete_list(tmp_length, tmp_list);

    s3_get_object_list(s3_iter->bkt_data.list[0], &s3_iter->obj_data.length,
                       &s3_iter->obj_data.list);

    s3_iter->obj_data.current_id = -1;
    s3_iter->bkt_data.current_id = 0;
    s3_iter->values = rbh_sstack_new(1 << 10);

    return (struct rbh_mut_iterator *) s3_iter;
}
