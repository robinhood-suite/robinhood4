# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

from rbhpolicy.config.conditions import ConditionBuilder

# conditions init
Path             = ConditionBuilder("Path")
Name             = ConditionBuilder("Name")
IName            = ConditionBuilder("IName")
Type             = ConditionBuilder("Type")
User             = ConditionBuilder("User")
UID              = ConditionBuilder("UID")
Group            = ConditionBuilder("Group")
GID              = ConditionBuilder("GID")
Size             = ConditionBuilder("Size")
DirCount         = ConditionBuilder("DirCount")
LastAccess       = ConditionBuilder("LastAccess")
LastModification = ConditionBuilder("LastModification")
LastChange       = ConditionBuilder("LastChange")
CreationDate     = ConditionBuilder("CreationDate")
Pool             = ConditionBuilder("Pool")
