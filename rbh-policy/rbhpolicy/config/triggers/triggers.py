#!/usr/bin/env python3
# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

from abc import ABC, abstractmethod
from dataclasses import dataclass
from datetime import datetime
from typing import Optional, TypeVar
import re

T = TypeVar("T", bound="BaseTrigger")

@dataclass(frozen=True)
class TriggerContext:
    manual_mode: bool = False
    now: Optional[datetime] = None # useful for testing
    filesystem: Optional[str] = None

@dataclass(frozen=True)
class TriggerDecision:
    matched: bool
    reason: str

class BaseTrigger(ABC):
    """Base abstraction for all policy triggers."""

    def __and__(self, other: T) -> T:
        return TriggerAnd(self, other)

    def __or__(self, other: T) -> T:
        return TriggerOr(self, other)

    @abstractmethod
    def evaluate(self, context: TriggerContext) -> TriggerDecision:
        raise NotImplementedError

# useful for testing
class AlwaysTrigger(BaseTrigger):
    def evaluate(self, context: TriggerContext) -> TriggerDecision:
        return TriggerDecision(
            matched=True,
            reason="AlwaysTrigger always matches",
        )

class TriggerAnd(BaseTrigger):
    def __init__(self, left: BaseTrigger, right: BaseTrigger):
        self.left = left
        self.right = right

    def evaluate(self, context: TriggerContext) -> TriggerDecision:
        left_decision = self.left.evaluate(context)
        if not left_decision.matched:
            return left_decision

        return self.right.evaluate(context)

class TriggerOr(BaseTrigger):
    def __init__(self, left: BaseTrigger, right: BaseTrigger):
        self.left = left
        self.right = right

    def evaluate(self, context: TriggerContext) -> TriggerDecision:
        left_decision = self.left.evaluate(context)
        if left_decision.matched:
            return left_decision

        return self.right.evaluate(context)

def normalize_trigger(trigger) -> BaseTrigger:
    if trigger is None:
        return AlwaysTrigger()

    if isinstance(trigger, BaseTrigger):
        return trigger

    raise TypeError("trigger must be a BaseTrigger instance or None")

def evaluate_trigger(trigger, context: TriggerContext) -> TriggerDecision:
    if context is None:
        raise ValueError("TriggerContext must be provided explicitly")
    trigger_obj = normalize_trigger(trigger)
    return trigger_obj.evaluate(context)

# ============================================================================
# Time-based triggers: Periodic and Scheduled
# ============================================================================

def _parse_duration(duration_str: str) -> int:
    """
    Parse duration string like '10m', '1h', '5d' into seconds.
    Supported: s (seconds), m (minutes), h (hours), d (days), mo (months), y
    (years).
    """
    m = re.match(r"^(\d+)(mo|[smhdy])$", duration_str.strip())
    if not m:
        raise ValueError(
            f"Invalid duration format: {duration_str!r}. "
            "Expected format: <number><unit> (e.g., '10m', '1h', "
            "'5d', '2mo', '1y')"
        )

    value, unit = int(m.group(1)), m.group(2)
    multipliers = {
        "s": 1,
        "m": 60,
        "h": 3600,
        "d": 86400,
        "mo": 30 * 86400,
        "y": 365 * 86400,
    }
    return value * multipliers[unit]

class PeriodicTrigger(BaseTrigger):
    """Trigger that matches at regular intervals."""

    def __init__(self, interval_seconds: int):
        if interval_seconds <= 0:
            raise ValueError("interval_seconds must be positive")
        self.interval_seconds = interval_seconds

    def evaluate(self, context: TriggerContext) -> TriggerDecision:
        # In manual mode, temporal triggers should not block execution
        if context.manual_mode:
            return TriggerDecision(
                matched=True,
                reason="periodic trigger ignored in manual mode",
            )

        # Minimal behavior for now: always match waiting for deamon implem
        return TriggerDecision(
            matched=True,
            reason=f"periodic trigger (interval={self.interval_seconds}s)",
        )

def _parse_datetime(datetime_str: str) -> datetime:
    """
    Parse datetime string in format 'YYYY-MM-DD HH:MM:SS' or 'YYYY-MM-DD HH:MM'.
    """
    dt_str = datetime_str.strip()
    for fmt in ["%Y-%m-%d %H:%M:%S", "%Y-%m-%d %H:%M"]:
        try:
            return datetime.strptime(dt_str, fmt)
        except ValueError:
            continue
    raise ValueError(
        f"Invalid datetime format: {datetime_str!r}. "
        "Expected format: 'YYYY-MM-DD HH:MM' or 'YYYY-MM-DD HH:MM:SS'"
    )

class ScheduledTrigger(BaseTrigger):
    """Trigger that matches at a specific datetime."""

    def __init__(self, target_datetime: datetime):
        self.target_datetime = target_datetime

    def evaluate(self, context: TriggerContext) -> TriggerDecision:
        if context.manual_mode:
            return TriggerDecision(
                matched=True,
                reason="scheduled trigger ignored in manual mode",
            )

        now = context.now if context.now is not None else datetime.now()
        if now >= self.target_datetime:
            return TriggerDecision(
                matched=True,
                reason=(f"scheduled trigger matched (target: "
                        f"{self.target_datetime})"),
            )
        else:
            return TriggerDecision(
                matched=False,
                reason=(f"scheduled trigger not yet matched (target: "
                        f"{self.target_datetime})"),
            )

# ============================================================================
# DSL functions for creating temporal triggers
# ============================================================================

def Periodic(interval_str: str) -> PeriodicTrigger:
    """
    Create a periodic trigger from a duration string like '10m', '1h', '5d',
    '2mo', '1y'.
    """
    interval_seconds = _parse_duration(interval_str)
    return PeriodicTrigger(interval_seconds)


def Scheduled(datetime_str: str) -> ScheduledTrigger:
    """
    Create a scheduled trigger from a datetime string like '2025-06-01 03:00'.
    """
    target_dt = _parse_datetime(datetime_str)
    return ScheduledTrigger(target_dt)
