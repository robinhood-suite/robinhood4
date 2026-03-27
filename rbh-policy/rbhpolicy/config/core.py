#!/usr/bin/env python3
# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

from rbhpolicy.config.triggers.triggers import (
    Periodic,
    Scheduled,
    AlwaysTrigger,
    GlobalSizePercent,
    GlobalInodePercent,
    UserFileCount,
    UserDiskUsage,
    UserInodeCount,
    GroupFileCount,
    GroupDiskUsage,
    GroupInodeCount,
)
from rbhpolicy.config.fileclass import declare_fileclass
from rbhpolicy.config.policy import declare_policy, Rule
from rbhpolicy.config.cpython import config
from rbhpolicy.config.utils import cmd
from rbhpolicy.config.entities import *
from rbhpolicy.config.core_actions import *

# Convenient trigger shortcuts for common intervals
EveryMinute = Periodic("1m")
Hourly = Periodic("1h")
Daily = Periodic("1d")
Weekly = Periodic("7d")
Monthly = Periodic("30d")

Always = AlwaysTrigger()
