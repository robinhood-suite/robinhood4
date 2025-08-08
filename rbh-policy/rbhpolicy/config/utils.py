# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import re
from datetime import datetime

def parse_storage_unit(value: str):
    """Parse storage size like '10MB' to find-compatible format."""
    units = {
        "B":  ("c", 1),
        "KB": ("k", 1),
        "MB": ("M", 1),
        "GB": ("G", 1),
        "TB": ("T", 1),
        "PB": ("T", 1024),
        "EB": ("T", 1024 * 1024)
    }

    match = re.match(r"^(\d+)([a-zA-Z]+)$", value)
    if not match:
        raise ValueError(f"Invalid storage format: '{value}'")

    number, unit = match.groups()
    if unit not in units:
        raise ValueError(f"Unsupported unit: '{unit}'")

    suffix, factor = units[unit]
    return f"{int(number) * factor}{suffix}"


def parse_file_type(value: str):
    """Convert file type string like 'file' to find-compatible flag."""
    types = {
        "file": "f",
        "dir": "d",
        "symlink": "l",
        "block": "b",
        "char": "c",
        "fifo": "p",
        "socket": "s"
    }

    key = value.lower()
    if key not in types:
        raise ValueError(f"Unknown file type: '{value}'")

    return types[key]


def parse_time_unit(value: str):
    """Parse time like '2h' or '1w' to days or minutes."""
    units = {
        "m": 1,
        "h": 60,
        "d": 1440,
        "w": 10080,
        "mo": 43200,
        "y": 525600
    }

    match = re.match(r"^(\d+)(m|h|d|w|mo|y)$", value)
    if not match:
        raise ValueError(f"Invalid time format: '{value}'")

    amount, unit = match.groups()
    total_minutes = int(amount) * units[unit]

    if total_minutes < 1440:
        return str(total_minutes), "m"
    else:
        return str(total_minutes // 1440), "d"

def parse_quantity_unit(value: str):
    """Parse quantity like '100k' or '2Bn' into integer string."""
    units = {
        "k": 1_000,
        "M": 1_000_000,
        "Bn": 1_000_000_000,
        "Tn": 1_000_000_000_000
    }

    match = re.match(r"^(\d+)(k|M|Bn|Tn)$", value)
    if not match:
        raise ValueError(f"Invalid quantity format: '{value}'")

    number, unit = match.groups()
    return str(int(number) * units[unit])

def parse_int(value: str):
    """Accept plain integers without any unit."""
    if re.fullmatch(r"\d+", value):
        return value
    raise ValueError(f"Not a plain integer: '{value}'")


def parse_date(value: str):
    """Parse human-readable date into datetime string."""
    known_formats = [
        "%Y-%m-%d",        # 2024-07-30
        "%d/%m/%Y",        # 30/07/2024
        "%d-%m-%Y",        # 30-07-2024
        "%Y/%m/%d",        # 2024/07/30
        "%Y.%m.%d",        # 2024.07.30
        "%d.%m.%Y",        # 30.07.2024
        "%Y-%m-%dT%H:%M:%S", # 2024-07-30T13:45:00
    ]

    for fmt in known_formats:
        try:
            dt = datetime.strptime(value, fmt)
            return dt.isoformat()
        except ValueError:
            continue

    raise ValueError(f"Invalid date format: '{value}'")

def parse_unit(value: str):
    """
    Parse a string representing a value with optional unit.

    Tries to detect and convert the value as one of:
    - storage size (e.g. "10MB" → "10M")
    - quantity (e.g. "100k" → "100000")
    - time (e.g. "2h" → "120", with suffix "m" or "d")
    - file type (e.g. "dir" → "d")
    - plain integer (e.g. "42" → "42")
    - date (e.g. "2025-07-30" → "2025-07-30T00:00:00")

    Returns:
        tuple[str, str, str | None]: (value, kind, unit)
        - value: the normalized string (e.g. "100000", "10M", "d")
        - kind: one of "storage", "quantity", "time", "file_type", "int"
        - unit:
            - "m" if time is returned in minutes
            - "d" if time is returned in days
            - None for all other kinds
    """
    parsers = [
        ("storage", parse_storage_unit),
        ("quantity", parse_quantity_unit),
        ("time", parse_time_unit),
        ("file_type", parse_file_type),
        ("int", parse_int),
        ("date", parse_date)
    ]

    for kind, parser in parsers:
        try:
            result = parser(value)
            if kind == "time":
                val, suffix = result
                return val, kind, suffix
            else:
                return result, kind, None
        except ValueError:
            continue

    raise ValueError(f"No unit recognized in '{value}'")

def split_number_suffix(val: str):
    m = re.match(r"^(\d+)([A-Za-z]*)$", val)
    if not m:
        raise ValueError(f"Invalid numeric+suffix format: '{val}'")
    num = int(m.group(1))
    suffix = m.group(2) or ""
    return num, suffix

def translate_condition(key: str, operator: str, value: str):
    keys_without_units = {"Path", "Name", "IName", "User", "Group", "Pool"}
    option_days = {
        "Path": "-path",
        "Name": "-name",
        "IName": "-iname",
        "Type": "-type",
        "User": "-user",
        "UID": "-uid",
        "Group": "-group",
        "GID": "-gid",
        "Size": "-size",
        "DirCount": "-dircount",
        "LastAccess": "-atime",
        "LastModification": "-mtime",
        "LastChange": "-ctime",
        "CreationDate": "-btime",
        "Pool": "-pool",
    }

    option_minutes = {
        "LastAccess": "-amin",
        "LastModification": "-mmin",
        "LastChange": "-cmin",
        "CreationDate": "-bmin",
    }

    if key not in option_days:
        raise ValueError(f"Unknown key '{key}'")

    if key in keys_without_units:
        return [option_days[key], value]

    parsed_val, kind, unit = parse_unit(value)

    if key in option_minutes and kind == "time" and unit == "m":
        option = option_minutes[key]
    else:
        option = option_days[key]

    if key == "Type":
        return [option, parsed_val]

    if key == "Size":
        if kind == "int":
            parsed_val = f"{parsed_val}c"
            kind = "storage"
        num, suffix = split_number_suffix(parsed_val)

        if operator == "eq":
            final = f"{num}{suffix}"
        elif operator == "gt":
            final = f"+{num}{suffix}"
        elif operator == "ge":
            final = f"+{max(num-1, 0)}{suffix}"
        elif operator == "lt":
            final = f"-{num}{suffix}"
        elif operator == "le":
            final = f"-{num+1}{suffix}"
        else:
            raise ValueError(f"Unsupported operator for Size: {operator}")

        return [option, final]

    # Prototype because not yet implemented in rbh
    if key == "DirCount":
        num = int(parsed_val)
        if operator == "eq":
            final = str(num)
        elif operator == "gt":
            final = f"+{num}"
        elif operator == "ge":
            final = f"+{max(num-1, 0)}"
        elif operator == "lt":
            final = f"-{num}"
        elif operator == "le":
            final = f"-{num+1}"
        else:
            raise ValueError(f"Unsupported operator for DirCount: {operator}")
        return [option, final]

    if kind == "time" and key in option_days:
        num = int(parsed_val)
        if operator == "eq":
            final = str(num)
        elif operator == "gt":
            final = f"+{num}"
        elif operator == "ge":
            final = f"+{max(num-1, 0)}"
        elif operator == "lt":
            final = f"-{num}"
        elif operator == "le":
            final = f"-{num+1}"
        else:
            raise ValueError(f"Unsupported operator for time: {operator}")
        return [option, final]

    if kind == "date":
        return [option, parsed_val]

    return [option, value]
