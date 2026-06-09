#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later


################################################################################
#                               SELINUX UTILS                                  #
################################################################################


require_selinux()
{
    command -v chcon >/dev/null || skip "chcon is required"
    command -v getfattr >/dev/null || skip "getfattr is required"

    if command -v getenforce >/dev/null && [[ "$(getenforce)" == "Disabled" ]]; then
        skip "SELinux is disabled"
    fi
}

get_selinux_context()
{
    getfattr -n security.selinux -e text "$1" 2>/dev/null |
        sed -n '/^security\.selinux=/p' |
        sed 's/^security\.selinux="//; s/"$//'
}

set_selinux_range_or_skip()
{
    local range="$1"
    local file="$2"
    local ctx

    sudo chcon -l "$range" "$file" 2>/dev/null ||
        skip "failed to set SELinux range '$range' on '$file'"

    ctx=$(get_selinux_context "$file")

    if [[ "$ctx" != *":$range" ]]; then
        skip "SELinux range '$range' was not applied to '$file': '$ctx'"
    fi
}

rbh_sync_selinux()
{
    rbh_sync "rbh:selinux:$1" "$2" $3
}
