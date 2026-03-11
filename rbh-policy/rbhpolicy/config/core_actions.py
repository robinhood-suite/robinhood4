#!/usr/bin/env python3
# This file is part of RobinHood 4
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

class action:
    log = "log"
    delete = "delete"
    # Add more built-in actions as needed

class _ExtActions:
    """
    Namespace for extension-specific built-in actions.

    Each attribute is a string of the form "<ext>:<builtin>" that the C
    policy engine uses to select the common_ops of the named extension.

    Usage in a config file::

        action = lustre.delete  # forces Lustre extension's delete
        action = retention.log  # forces retention extension's log
    """
    def __init__(self, ext_name: str):
        for builtin in ("log", "delete"):
            setattr(self, builtin, f"{ext_name}:{builtin}")


lustre    = _ExtActions("lustre")
retention = _ExtActions("retention")
posix     = _ExtActions("posix")
