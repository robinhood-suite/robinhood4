/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 * 		      alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifndef ROBINHOOD_POSIX_BACKEND_H
#define ROBINHOOD_POSIX_BACKEND_H

#include "robinhood/backend.h"

#define RBH_POSIX_BACKEND_NAME "posix"
#define RBH_POSIX_BACKEND_VERSION RPV(0, 0, 0)

/**
 * Create a posix backend
 *
 * @param path      a path to the root fsentry
 *
 * @return          a pointer to a newly allocated posix backend on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error ENOMEM    there was not enough memory available
 */
struct rbh_backend *
rbh_posix_backend_new(const char *path);

#endif
