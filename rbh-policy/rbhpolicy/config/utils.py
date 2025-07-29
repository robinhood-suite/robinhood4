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
