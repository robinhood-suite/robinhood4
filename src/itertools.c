/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 * 		      alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>

#include "robinhood/itertools.h"

/*----------------------------------------------------------------------------*
 |                            rbh_array_iterator()                            |
 *----------------------------------------------------------------------------*/

struct array_iterator {
    struct rbh_iterator iter;

    const void *array;
    size_t size;
    size_t count;

    size_t index;
};

static const void *
array_iter_next(void *iterator)
{
    struct array_iterator *array = iterator;

    if (array->index < array->count)
        return (const char *)array->array + (array->size * array->index++);

    errno = ENODATA;
    return NULL;
}

static void
array_iter_destroy(void *iterator)
{
    free(iterator);
}

static const struct rbh_iterator_operations ARRAY_ITER_OPS = {
    .next = array_iter_next,
    .destroy = array_iter_destroy,
};

static const struct rbh_iterator ARRAY_ITER = {
    .ops = &ARRAY_ITER_OPS,
};

struct rbh_iterator *
rbh_array_iterator(const void *array, size_t element_size, size_t element_count)
{
    struct array_iterator *iterator;

    iterator = malloc(sizeof(*iterator));
    if (iterator == NULL)
        return NULL;
    iterator->iter = ARRAY_ITER;

    iterator->array = array;
    iterator->size = element_size;
    iterator->count = element_count;

    iterator->index = 0;

    return &iterator->iter;
}

/*----------------------------------------------------------------------------*
 |                           rbh_mut_array_iterator                           |
 *----------------------------------------------------------------------------*/

struct rbh_mut_iterator *
rbh_mut_array_iterator(void *array, size_t element_size, size_t element_count)
{
    return (struct rbh_mut_iterator *)rbh_array_iterator(array, element_size,
                                                         element_count);
}
