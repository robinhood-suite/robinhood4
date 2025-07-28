# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

from rbhpolicy.config.conditions import ConditionBuilder

# conditions init
CreationDate     = ConditionBuilder("CreationDate")
DirCount         = ConditionBuilder("DirCount")
GID              = ConditionBuilder("GID")
Group            = ConditionBuilder("Group")
IName            = ConditionBuilder("IName")
LastAccess       = ConditionBuilder("LastAccess")
LastChange       = ConditionBuilder("LastChange")
LastModification = ConditionBuilder("LastModification")
Name             = ConditionBuilder("Name")
Path             = ConditionBuilder("Path")
Pool             = ConditionBuilder("Pool")
Size             = ConditionBuilder("Size")
Type             = ConditionBuilder("Type")
UID              = ConditionBuilder("UID")
User             = ConditionBuilder("User")
