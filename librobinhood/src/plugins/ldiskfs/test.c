/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <ext2fs/ext2fs.h>

int main(int argc, char **argv) {
    printf("%s\n", "stdout");
    fprintf(stderr, "%s\n", "stderr");
    return 0;
}
