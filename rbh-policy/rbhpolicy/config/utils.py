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
        "B":  "c",
        "c":  "c",
        "KB": "k",
        "k":  "k",
        "MB": "M",
        "M":  "M",
        "GB": "G",
        "G":  "G",
        "TB": "T",
        "T":  "T",
        "PB": "P",
        "P":  "P",
        "EB": "E",
        "E":  "E",
    }

    match = re.match(r"^(\d+)([a-zA-Z]+)$", value)
    if not match:
        raise ValueError(f"Invalid storage format: '{value}'")

    number, unit = match.groups()
    if unit not in units:
        raise ValueError(f"Unsupported unit: '{unit}'")

    return f"{number}{units[unit]}"

def parse_file_type(value: str):
    """Convert file type string like 'file' to find-compatible flag."""
    types = {
        "file": "f",
        "f": "f",
        "dir": "d",
        "d": "d",
        "symlink": "l",
        "l": "l",
        "block": "b",
        "b": "b",
        "char": "c",
        "c": "c",
        "fifo": "p",
        "p": "p",
        "socket": "s",
        "s": "s"
    }

    key = value.lower()
    if key not in types:
        raise ValueError(f"Unknown file type: '{value}'")

    return types[key]

def parse_time_unit(value: str):
    """Parse time like '2h' or '1w' to days or minutes."""
    units = {
        "m": 1,
        "minute": 1,
        "h": 60,
        "hour": 60,
        "d": 1440,
        "day": 1440,
        "w": 10080,
        "week": 10080,
        "mo": 43200,
        "month": 43200,
        "y": 525600,
        "year": 525600
    }

    matches = re.findall(
        r"(\d+)\s*(m|h|d|w|mo|y|minute|hour|day|week|month|year)",
        value,
        re.IGNORECASE
    )

    if not matches:
        raise ValueError(f"Invalid time format: '{value}'")

    total_minutes = 0
    for amount, unit in matches:
        unit = unit.lower()
        total_minutes += int(amount) * units[unit]

    if total_minutes < 1440:
        return str(total_minutes), "m"
    else:
        return str(total_minutes // 1440), "d"

def parse_quantity_unit(value: str):
    """Parse quantity like '100k' or '2Bn' into integer string."""
    units = {
        "k": 1_000,
        "Mn": 1_000_000,
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
        "%Y-%m-%d",            # 2024-07-30 ISO
        "%Y-%m-%dT%H:%M:%S",   # 2024-07-30T13:45:00 ISO

        "%d/%m/%Y",            # 30/07/2024
        "%d-%m-%Y",            # 30-07-2024
        "%Y/%m/%d",            # 2024/07/30

        "%Y.%m.%d",            # 2024.07.30
        "%d.%m.%Y",            # 30.07.2024
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
