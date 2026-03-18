#!/usr/bin/env python3
# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

import os
from typing import Optional

from rbhpolicy.config.triggers.capacity_providers import (
    CapacityMetrics,
    CapacityProvider,
    register_capacity_provider_resolver,
)
from rbhpolicy.config.cpython import resolve_backend_type

class PosixCapacityProvider(CapacityProvider):
    """Capacity provider for POSIX-backed filesystems."""

    def __init__(self, mount_path: str):
        if not os.path.isabs(mount_path):
            raise ValueError("mount_path must be an absolute path")
        self.mount_path = mount_path

    def get_capacity(self) -> CapacityMetrics:
        vfs = os.statvfs(self.mount_path)
        total_bytes = vfs.f_blocks * vfs.f_frsize
        free_bytes = vfs.f_bfree * vfs.f_frsize
        used_bytes = total_bytes - free_bytes

        total_inodes = vfs.f_files
        free_inodes = vfs.f_ffree
        used_inodes = total_inodes - free_inodes

        return CapacityMetrics(
            used_bytes=max(0, used_bytes),
            total_bytes=max(0, total_bytes),
            used_inodes=max(0, used_inodes),
            total_inodes=max(0, total_inodes),
        )

def _extract_posix_path(filesystem: Optional[str]) -> Optional[str]:
    if not filesystem:
        return None

    if filesystem.startswith("rbh:"):
        # Parse the URI: rbh:<backend_name>:<path>
        rest = filesystem[4:]  # strip "rbh:"
        colon = rest.find(":")
        if colon == -1:
            return None
        backend_name = rest[:colon]
        path = rest[colon + 1:]

        resolved_type = resolve_backend_type(backend_name)
        if resolved_type == "posix" and os.path.isabs(path):
            return path
        return None

    if os.path.isabs(filesystem):
        return filesystem

    return None

def _resolve_posix_provider(filesystem: Optional[str]) -> Optional[CapacityProvider]:
    path = _extract_posix_path(filesystem)
    if path is None:
        return None
    return PosixCapacityProvider(path)

register_capacity_provider_resolver(_resolve_posix_provider)
