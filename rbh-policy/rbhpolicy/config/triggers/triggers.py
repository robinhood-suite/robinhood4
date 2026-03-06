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
