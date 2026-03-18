#!/usr/bin/env python3
# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Callable, List, Optional

@dataclass(frozen=True)
class CapacityMetrics:
    used_bytes: int
    total_bytes: int
    used_inodes: int
    total_inodes: int

class CapacityProvider(ABC):
    """Abstraction to retrieve capacity metrics for trigger evaluation."""

    @abstractmethod
    def get_capacity(self) -> CapacityMetrics:
        raise NotImplementedError

class UnsupportedCapacityProvider(CapacityProvider):
    """
    Explicit provider used when no capacity backend is currently available.
    """

    def __init__(self, reason: str):
        self.reason = reason

    def get_capacity(self) -> CapacityMetrics:
        raise RuntimeError(self.reason)

CapacityResolver = Callable[[Optional[str]], Optional[CapacityProvider]]

_CAPACITY_RESOLVERS: List[CapacityResolver] = []

def register_capacity_provider_resolver(resolver: CapacityResolver) -> None:
    """Register a resolver that can return a provider for a database URI."""
    _CAPACITY_RESOLVERS.append(resolver)

def capacity_provider_from_filesystem(database_uri: Optional[str]) -> CapacityProvider:
    """
    Build a capacity provider from a database URI.

    Resolvers are plugin-like and registered by backend-specific modules.
    """
    for resolver in _CAPACITY_RESOLVERS:
        provider = resolver(database_uri)
        if provider is not None:
            return provider

    return UnsupportedCapacityProvider(
        f"unsupported database URI for capacity provider: {database_uri!r}"
    )
