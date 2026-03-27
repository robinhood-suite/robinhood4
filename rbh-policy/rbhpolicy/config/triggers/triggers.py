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

from rbhpolicy.config import cpython as rbh
from rbhpolicy.config.entities import Group, Type, User
from rbhpolicy.config.triggers.capacity_providers import (
    capacity_provider_from_filesystem,
)
from rbhpolicy.config.triggers import posix_capacity_provider

VALID_OPERATORS = (">", "<", ">=", "<=", "==", "!=")
T = TypeVar("T", bound="BaseTrigger")

@dataclass(frozen=True)
class TriggerContext:
    manual_mode: bool = False
    now: Optional[datetime] = None # useful for testing
    database_uri: Optional[str] = None

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

def _parse_selector_list(raw_selectors) -> list[str]:
    if isinstance(raw_selectors, str):
        selectors = [s.strip() for s in raw_selectors.split(",")]
    elif isinstance(raw_selectors, (list, tuple, set)):
        selectors = [str(s).strip() for s in raw_selectors]
    else:
        raise TypeError("selectors must be a comma-separated string or "
                        "an iterable of strings")

    selectors = [s for s in selectors if s]
    if not selectors:
        raise ValueError("selectors cannot be empty")

    return selectors

def _compare_numeric(value: float, threshold: float, operator: str) -> bool:
    if operator == ">":
        return value > threshold
    if operator == "<":
        return value < threshold
    if operator == ">=":
        return value >= threshold
    if operator == "<=":
        return value <= threshold
    if operator == "==":
        return value == threshold
    if operator == "!=":
        return value != threshold
    raise ValueError(f"invalid operator: {operator}")

def _parse_size_to_bytes(value) -> int:
    if isinstance(value, int):
        if value < 0:
            raise ValueError("size threshold must be >= 0")
        return value

    if not isinstance(value, str):
        raise TypeError("size threshold must be an int or a string like '5TB'")

    value = value.strip()

    # Same unit logic as parse_storage_unit (case-sensitive)
    units = {
        "B": 1,
        "c": 1,
        "KB": 1024,
        "k": 1024,
        "MB": 1024**2,
        "M": 1024**2,
        "GB": 1024**3,
        "G": 1024**3,
        "TB": 1024**4,
        "T": 1024**4,
        "PB": 1024**5,
        "P": 1024**5,
        "EB": 1024**6,
        "E": 1024**6,
    }

    match = re.fullmatch(r"(\d+)([A-Za-z]+)", value)
    if not match:
        raise ValueError(f"invalid size format: '{value}'")

    magnitude = int(match.group(1))
    unit = match.group(2)

    if unit not in units:
        raise ValueError(f"unsupported size unit: '{unit}'")

    return magnitude * units[unit]

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

# ============================================================================
# Capacity-based triggers: GlobalSizePercent and GlobalInodePercent
# ============================================================================

class _BasePercentTrigger(BaseTrigger):
    metric_name: str
    get_used: callable
    get_total: callable
    zero_reason: str

    def __init__(self, threshold: float, operator: str = ">="):
        if not (0.0 <= threshold <= 100.0):
            raise ValueError("threshold must be between 0 and 100")
        if operator not in (">", "<", ">=", "<=", "==", "!="):
            raise ValueError(f"invalid operator: {operator}")
        self.threshold = threshold
        self.operator = operator

    def evaluate(self, context: TriggerContext) -> TriggerDecision:
        provider = capacity_provider_from_filesystem(context.database_uri)
        metrics = provider.get_capacity()

        total = self.get_total(metrics)
        if total == 0:
            return TriggerDecision(matched=False, reason=self.zero_reason)

        used_pct = self.get_used(metrics) / total * 100.0

        # Compare using operator
        matched = _compare_numeric(used_pct, self.threshold, self.operator)

        return TriggerDecision(
            matched=matched,
            reason=(
                f"{self.metric_name}: {used_pct:.1f}% used "
                f"({self.operator} threshold {self.threshold}%): "
                f"{'matched' if matched else 'not matched'}"
            ),
        )
class GlobalSizePercentTrigger(_BasePercentTrigger):
    metric_name = "GlobalSizePercent"
    get_used = staticmethod(lambda m: m.used_bytes)
    get_total = staticmethod(lambda m: m.total_bytes)
    zero_reason = ("GlobalSizePercent: total_bytes is 0 (filesystem not "
                  "available or not mounted)")

class GlobalInodePercentTrigger(_BasePercentTrigger):
    metric_name = "GlobalInodePercent"
    get_used = staticmethod(lambda m: m.used_inodes)
    get_total = staticmethod(lambda m: m.total_inodes)
    zero_reason = ("GlobalInodePercent: total_inodes is 0 (filesystem not "
                   "available or inode counting not supported)")

# ============================================================================
# Comparator classes for operator overloading (GlobalSizePercent,
#                                              GlobalInodePercent)
# ============================================================================

class _SizePercentComparator:
    """
    Allows syntax like: GlobalSizePercent > 90, GlobalSizePercent <= 80, etc.
    """

    def __gt__(self, threshold: float) -> GlobalSizePercentTrigger:
        """GlobalSizePercent > threshold"""
        return GlobalSizePercentTrigger(threshold, operator=">")

    def __lt__(self, threshold: float) -> GlobalSizePercentTrigger:
        """GlobalSizePercent < threshold"""
        return GlobalSizePercentTrigger(threshold, operator="<")

    def __ge__(self, threshold: float) -> GlobalSizePercentTrigger:
        """GlobalSizePercent >= threshold"""
        return GlobalSizePercentTrigger(threshold, operator=">=")

    def __le__(self, threshold: float) -> GlobalSizePercentTrigger:
        """GlobalSizePercent <= threshold"""
        return GlobalSizePercentTrigger(threshold, operator="<=")

    def __eq__(self, threshold: float) -> GlobalSizePercentTrigger:
        """GlobalSizePercent == threshold"""
        return GlobalSizePercentTrigger(threshold, operator="==")

    def __ne__(self, threshold: float) -> GlobalSizePercentTrigger:
        """GlobalSizePercent != threshold"""
        return GlobalSizePercentTrigger(threshold, operator="!=")

    def __call__(self, threshold: float) -> GlobalSizePercentTrigger:
        """Legacy syntax: GlobalSizePercent(80) for backwards compatibility"""
        return GlobalSizePercentTrigger(threshold, operator=">=")

class _InodePercentComparator:
    """
    Allows syntax like: GlobalInodePercent > 90, GlobalInodePercent <= 80, etc.
    """

    def __gt__(self, threshold: float) -> GlobalInodePercentTrigger:
        """GlobalInodePercent > threshold"""
        return GlobalInodePercentTrigger(threshold, operator=">")

    def __lt__(self, threshold: float) -> GlobalInodePercentTrigger:
        """GlobalInodePercent < threshold"""
        return GlobalInodePercentTrigger(threshold, operator="<")

    def __ge__(self, threshold: float) -> GlobalInodePercentTrigger:
        """GlobalInodePercent >= threshold"""
        return GlobalInodePercentTrigger(threshold, operator=">=")

    def __le__(self, threshold: float) -> GlobalInodePercentTrigger:
        """GlobalInodePercent <= threshold"""
        return GlobalInodePercentTrigger(threshold, operator="<=")

    def __eq__(self, threshold: float) -> GlobalInodePercentTrigger:
        """GlobalInodePercent == threshold"""
        return GlobalInodePercentTrigger(threshold, operator="==")

    def __ne__(self, threshold: float) -> GlobalInodePercentTrigger:
        """GlobalInodePercent != threshold"""
        return GlobalInodePercentTrigger(threshold, operator="!=")

    def __call__(self, threshold: float) -> GlobalInodePercentTrigger:
        """Legacy syntax: GlobalInodePercent(90) for backwards compatibility"""
        return GlobalInodePercentTrigger(threshold, operator=">=")

# ============================================================================
# DSL functions for creating capacity-based triggers
# ============================================================================

GlobalSizePercent = _SizePercentComparator()
GlobalInodePercent = _InodePercentComparator()

# ============================================================================
# Database-based triggers: MirrorUserFileCount, MirrorUserDiskUsage,
#                          MirrorUserInodeCount, MirrorGroupFileCount,
#                          MirrorGroupDiskUsage, MirrorGroupInodeCount
# ============================================================================

class DbStatTrigger(BaseTrigger):
    """Generic trigger based on a DB aggregate stat query."""

    def __init__(self, *, label: str, rbh_filter, stat_field: int,
                 threshold: int, operator: str = ">="):
        if operator not in VALID_OPERATORS:
            raise ValueError(f"invalid operator: {operator}")
        if threshold < 0:
            raise ValueError("threshold must be >= 0")
        self.label = label
        self.rbh_filter = rbh_filter
        self.stat_field = stat_field
        self.threshold = threshold
        self.operator = operator

    def evaluate(self, context: TriggerContext) -> TriggerDecision:
        try:
            current = rbh.trigger_query_stat(self.rbh_filter, self.stat_field)
        except Exception as exc:  # pragma: no cover - defensive runtime guard
            return TriggerDecision(
                matched=False,
                reason=f"{self.label}: DB query failed ({exc})",
            )

        matched = _compare_numeric(current, self.threshold, self.operator)
        return TriggerDecision(
            matched=matched,
            reason=(
                f"{self.label}: {current} {self.operator} {self.threshold} "
                f"=> {'matched' if matched else 'not matched'}"
            ),
        )

class _DbStatComparator:
    """Comparator object used by the DSL factories for DB-based triggers."""

    def __init__(self, *, label: str, rbh_filter, stat_field: int,
                 threshold_parser):
        self.label = label
        self.rbh_filter = rbh_filter
        self.stat_field = stat_field
        self.threshold_parser = threshold_parser

    def _make_trigger(self, threshold, operator: str) -> DbStatTrigger:
        parsed_threshold = self.threshold_parser(threshold)
        return DbStatTrigger(
            label=self.label,
            rbh_filter=self.rbh_filter,
            stat_field=self.stat_field,
            threshold=parsed_threshold,
            operator=operator,
        )

    def __gt__(self, threshold) -> DbStatTrigger:
        return self._make_trigger(threshold, ">")

    def __lt__(self, threshold) -> DbStatTrigger:
        return self._make_trigger(threshold, "<")

    def __ge__(self, threshold) -> DbStatTrigger:
        return self._make_trigger(threshold, ">=")

    def __le__(self, threshold) -> DbStatTrigger:
        return self._make_trigger(threshold, "<=")

    def __eq__(self, threshold) -> DbStatTrigger:
        return self._make_trigger(threshold, "==")

    def __ne__(self, threshold) -> DbStatTrigger:
        return self._make_trigger(threshold, "!=")

def _parse_non_negative_int(value) -> int:
    if not isinstance(value, int):
        raise TypeError("threshold must be an integer")
    if value < 0:
        raise ValueError("threshold must be >= 0")
    return value

def _build_selector_filter(builder, selectors: list[str], only_files):
    combined = None
    for selector in selectors:
        condition = builder == selector
        selector_filter = condition.to_filter()
        combined = selector_filter if combined is None else \
            rbh.rbh_filter_or(combined, selector_filter)

    if only_files:
        file_only = (Type == "f").to_filter()
        return rbh.rbh_filter_and(combined, file_only)

    return combined

def _build_selector_stat_trigger(*, name: str, builder, raw_selectors,
                                 stat_field: int, threshold_parser,
                                 only_files=False):
    selectors = _parse_selector_list(raw_selectors)
    return _DbStatComparator(
        label=f"{name}[{','.join(selectors)}]",
        rbh_filter=_build_selector_filter(builder, selectors, only_files),
        stat_field=stat_field,
        threshold_parser=threshold_parser,
    )

# ============================================================================
# DSL functions for database-based triggers
# ============================================================================

_STAT_FIELD_SIZE = 0x00000200 # RBH_STATX_SIZE
_STAT_FIELD_COUNT = 0 # 0 => FA_COUNT in C trigger API.

def MirrorUserFileCount(users) -> _DbStatComparator:
    return _build_selector_stat_trigger(
        name="MirrorUserFileCount",
        builder=User,
        raw_selectors=users,
        stat_field=_STAT_FIELD_COUNT,
        threshold_parser=_parse_non_negative_int,
        only_files=True,
    )

def MirrorUserDiskUsage(users) -> _DbStatComparator:
    return _build_selector_stat_trigger(
        name="MirrorUserDiskUsage",
        builder=User,
        raw_selectors=users,
        stat_field=_STAT_FIELD_SIZE,
        threshold_parser=_parse_size_to_bytes,
    )

def MirrorUserInodeCount(users) -> _DbStatComparator:
    return _build_selector_stat_trigger(
        name="MirrorUserInodeCount",
        builder=User,
        raw_selectors=users,
        stat_field=_STAT_FIELD_COUNT,
        threshold_parser=_parse_non_negative_int,
    )

def MirrorGroupFileCount(groups) -> _DbStatComparator:
    return _build_selector_stat_trigger(
        name="MirrorGroupFileCount",
        builder=Group,
        raw_selectors=groups,
        stat_field=_STAT_FIELD_COUNT,
        threshold_parser=_parse_non_negative_int,
        only_files=True,
    )

def MirrorGroupDiskUsage(groups) -> _DbStatComparator:
    return _build_selector_stat_trigger(
        name="MirrorGroupDiskUsage",
        builder=Group,
        raw_selectors=groups,
        stat_field=_STAT_FIELD_SIZE,
        threshold_parser=_parse_size_to_bytes,
    )

def MirrorGroupInodeCount(groups) -> _DbStatComparator:
    return _build_selector_stat_trigger(
        name="MirrorGroupInodeCount",
        builder=Group,
        raw_selectors=groups,
        stat_field=_STAT_FIELD_COUNT,
        threshold_parser=_parse_non_negative_int,
    )
