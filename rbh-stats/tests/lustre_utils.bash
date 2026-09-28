#!/usr/bin/env bash

# This file is part of RobinHood 4.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

################################################################################
#                                DATABASE UTILS                                #
################################################################################


invoke_rbh-fsevents()
{
    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb"
}

################################################################################
#                                 LUSTRE UTILS                                 #
################################################################################

start_changelogs()
{
    local mdt="$1"

    echo "$(lctl --device "$mdt" changelog_register | cut -d "'" -f2)"
}

clear_changelogs()
{
    local mdt="$1"
    local user="$2"

    lfs changelog_clear "$mdt" "$user" 0
}

stop_changelogs()
{
    local mdt="$1"
    local user="$2"

    lctl --device "$mdt" changelog_deregister "$user"
}

lustre_setup()
{
    clear_changelogs "$LUSTRE_MDT" "$userid"
}

lustre_teardown()
{
    clear_changelogs "$LUSTRE_MDT" "$userid"
}
