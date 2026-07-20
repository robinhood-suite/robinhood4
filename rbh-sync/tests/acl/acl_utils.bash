#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later


################################################################################
#                                   ACL UTILS                                  #
################################################################################

set_access_acl()
{
    local acl="$1"
    local path="$2"

    setfacl -n -m "$acl" "$path" ||
        error "Failed to set access ACL '$acl' on '$path'"
}

set_default_acl()
{
    local acl="$1"
    local path="$2"

    setfacl -d -n -m "$acl" "$path" ||
        error "Failed to set default ACL '$acl' on '$path'"
}

rbh_sync_acl()
{
    rbh_sync "rbh:acl:$1" "$2"
}
