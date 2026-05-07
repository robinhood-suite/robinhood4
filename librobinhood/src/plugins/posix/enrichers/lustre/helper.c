/**
 * This file is part of RobinHood
 *
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 * alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood/config.h>

#include "lustre_internals.h"

void
rbh_lustre_helper(__attribute__((unused)) const char *backend,
                  __attribute__((unused)) struct rbh_config *config,
                  char **predicate_helper, char **directive_helper)
{
    int rc;

    rc = asprintf(predicate_helper,
        "  - Lustre:\n"
        "    -comp-count [+-]COUNT\n"
        "                         filter entries base on their component count.\n"
        "    -comp-end [+-]SIZE[,SIZE]\n"
        "                         filter entries based on their component's\n"
        "                         end values. `+` or `-` signs are\n"
        "                         not considered if given an interval in CSV.\n"
        "    -comp-flags {init, stale, prefer, prefrd, prefwr, offline, nosync, extension, parity, compress, partial, nocompr}\n"
        "                         filter entries based on their component's flags.\n"
        "    -comp-start [+-]SIZE[,SIZE]\n"
        "                         filter entries based on their component's\n"
        "                         start values. `+` or `-` signs are\n"
        "                         not considered if given an interval in CSV.\n"
        "    -composite           filter entries based on if they have a composite layout.\n"
        "    -fid FID             filter entries based on their FID.\n"
        "    -hash-type {all_char, fnv_1a_64, crush, crush2}\n"
        "                         filter entries based on their MDT hash type.\n"
        "    -hsm-state {archived, dirty, exists, lost, noarchive, none, norelease, released}\n"
        "                         filter entries based on their HSM state.\n"
        "    -layout-pattern {default, raid0, released, mdt, overstriped}\n"
        "                         filter entries based on the layout pattern\n"
        "                         of their components. If given default, will\n"
        "                         fetch the default pattern of the current\n"
        "                         Lustre FS and use it for filtering.\n"
        "    -mdt-count  [+-]COUNT\n"
        "                         filter entries based on their MDT count.\n"
        "    -mdt-index INDEX     filter entries based on the MDT they are on.\n"
        "    -mirror-count [+-]COUNT\n"
        "                         filter entries based on their mirror count.\n"
        "    -mirror-state {ro, sp, wp}\n"
        "                         filter entries based on their mirror state\n"
        "                           ro - the entry is in read-only state\n"
        "                           sp - the entry is in a state of being resynchronized\n"
        "                           wp - the entry is in a state of being written.\n"
        "    -ost-index INDEX     filter entries based on the OST they are on.\n"
        "    -pool NAME           filter entries based on the pool their\n"
        "                         components belong to (case sensitive, regex\n"
        "                         allowed).\n"
        "    -ipool NAME          filter entries based on the pool their\n"
        "                         components belong to (case insensitive,\n"
        "                         regex allowed).\n"
        "    -project-id INDEX    filter entries based on their project ID.\n"
        "    -stripe-count {[+-]COUNT, default}\n"
        "                         filter entries based on their component's\n"
        "                         stripe count. If given default, will fetch\n"
        "                         the default stripe count of the current\n"
        "                         Lustre FS and use it for filtering.\n"
        "    -stripe-size {[+-]SIZE, default}\n"
        "                         filter entries based on their component's\n"
        "                         stripe size. If given default, will fetch\n"
        "                         the default stripe size of the current\n"
        "                         Lustre FS and use it for filtering.\n"
        );

    if (rc == -1)
        *predicate_helper = NULL;

     rc = asprintf(directive_helper,
         "  - Lustre, directive category 'L':\n"
         "    %%RLb         component begins\n"
         "    %%RLc         component flags\n"
         "    %%RLC         OST or MDT count, depending on the entry's type\n"
         "    %%RLd         MDT hash flags\n"
         "    %%RLD         MDT hash type\n"
         "    %%RLe         component extension sizes\n"
         "    %%RLE         component ends\n"
         "    %%RLf         FID\n"
         "    %%RLF         parent FID\n"
         "    %%RLg         flags\n"
         "    %%RLh         HSM archive ID\n"
         "    %%RLH         HSM archive state\n"
         "    %%RLi         MDT index\n"
         "    %%RLm         mirror state\n"
         "    %%RLM         mirror count\n"
         "    %%RLn         generation\n"
         "    %%RLN         magic number\n"
         "    %%RLo         OSTs\n"
         "    %%RLp         project ID\n"
         "    %%RLP         pool\n"
         "    %%RLs         stripe size\n"
         "    %%RLS         stripe count\n"
         "    %%RLt         component pattern\n"
         "    %%RLT         component count\n"
         "    %%RLX         child MDT index\n"
     );

     if (rc == -1)
         *directive_helper = NULL;
}
