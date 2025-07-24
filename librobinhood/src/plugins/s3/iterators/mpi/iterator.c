/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
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

#include <robinhood/backends/s3_mpi.h>
#include <robinhood/backends/s3_extension.h>

#include "../../s3_internals.h"

#include "robinhood/backends/s3.h"
#include "robinhood/config.h"
#include "robinhood/mpi_rc.h"
#include "robinhood/sstack.h"
#include "robinhood/statx.h"

#include "../../s3_wrapper.h"

/*----------------------------------------------------------------------------*
 |                                s3_iterator                                 |
 *----------------------------------------------------------------------------*/

void
rbh_mpi_initialize(void)
{
    int initialized;

    MPI_Initialized(&initialized);
    if (!initialized) {
        MPI_Init(NULL, NULL);
    }
}

void
rbh_mpi_finalize(void)
{
    int initialized;
    int finalized;

    /* Prevents finalizing twice MPI if we use two backends with MPI */
    MPI_Initialized(&initialized);
    MPI_Finalized(&finalized);

    if (initialized && !finalized) {
        MPI_Finalize();
    }
}

static void
s3_mpi_iter_destroy(void *iterator)
{
    struct s3_iterator *s3_iter = iterator;

    s3_iter_destroy(s3_iter);
    rbh_mpi_dec_ref(rbh_mpi_finalize);

    free(s3_iter);
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
        *start = chunk_index * base_chunk_size;
    else
        *start = chunk_index * base_chunk_size + remainder;

    *end = *start + base_chunk_size - 1;

    return 0;
}

struct s3_iterator *
rbh_s3_mpi_iter_new()
{
    struct s3_iterator *s3_iter = NULL;
    size_t max_bucket_size = 64;
    char **temp_array;
    char **sub_array;
    char *sub_string;
    int arr_start;
    int rcv_size;
    int mpi_rank;
    int mpi_size;
    int sub_size;
    int arr_end;
    int rc;

    s3_iter = malloc(sizeof(*s3_iter));
    if (s3_iter == NULL)
        return NULL;

    rbh_mpi_inc_ref(rbh_mpi_initialize);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    if (mpi_rank == 0) {
        s3_get_bucket_list(&s3_iter->bkt_data.length, &s3_iter->bkt_data.list);
        s3_iter->obj_data.list = NULL;

        if (s3_iter->bkt_data.length == -1) {
            free(s3_iter);
            return NULL;
        }

        for (int i = 1; i < mpi_size; i++) {
            rc = get_sub_array_bounds(s3_iter->bkt_data.length, mpi_size, i,
                                      &arr_start, &arr_end);
            if (rc == -1) {
                sub_size = 0;
                MPI_Send(&sub_size, 1, MPI_INT, i, 0, MPI_COMM_WORLD);

            } else {
                sub_size = arr_end - arr_start + 1;
                MPI_Send(&sub_size, 1, MPI_INT, i, 0, MPI_COMM_WORLD);

                for (size_t j = arr_start; j <= arr_end; j++)
                    MPI_Send(s3_iter->bkt_data.list[j],
                             strlen(s3_iter->bkt_data.list[j]) + 1,
                             MPI_CHAR, i, 0, MPI_COMM_WORLD);
            }
        }
        rc = get_sub_array_bounds(s3_iter->bkt_data.length, mpi_size, 0,
                                  &arr_start, &arr_end);
        if (rc == 0) {
                sub_size = arr_end - arr_start + 1;
                temp_array = (char**)malloc(sub_size * sizeof(char*));
                if (temp_array == NULL)
                    return NULL;

                memcpy(temp_array ,s3_iter->bkt_data.list,
                       sub_size * sizeof(char*));

                s3_iter->bkt_data.list = temp_array;
                s3_iter->bkt_data.length = sub_size;
        } else {
                s3_iter->bkt_data.length = 0;
        }


    } else {
        MPI_Recv(&rcv_size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        sub_array = (char**)malloc(rcv_size * sizeof(char*));
        if (sub_array == NULL)
            return NULL;

        sub_string = malloc(max_bucket_size);
        for (int i = 0; i < rcv_size; i++) {
            sub_array[i] = (char*)malloc(max_bucket_size * sizeof(char));
            if (sub_array[i] == NULL)
                return NULL;

            MPI_Recv(sub_string, max_bucket_size, MPI_CHAR, 0, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            strcpy(sub_array[i], sub_string);
        }

        s3_iter->bkt_data.list = sub_array;
        s3_iter->bkt_data.length = rcv_size;
    }
    if (s3_iter->bkt_data.length > 0)
        s3_get_object_list(s3_iter->bkt_data.list[0], &s3_iter->obj_data.length,
                           &s3_iter->obj_data.list);

    s3_iter->obj_data.current_id = -1;
    s3_iter->bkt_data.current_id = 0;
    s3_iter->iterator = S3_MPI_ITER;

    s3_iter->values = rbh_sstack_new(1 << 10);
    if (s3_iter->values == NULL)
        return NULL;

    return (struct s3_iterator *)s3_iter;
}
