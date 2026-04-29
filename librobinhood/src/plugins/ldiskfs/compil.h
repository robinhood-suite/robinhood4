/* This file is part of RobinHood
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

/**
 * This file's only purpose is to be compiled by meson to check for the version
 * of ext2fs currently installed, whether it is the Kernel version or the Lustre
 * version, as it is important because some functions are defined differently
 * between the two.
 */
#include <ext2fs/ext2fs.h>
