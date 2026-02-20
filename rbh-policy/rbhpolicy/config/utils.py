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
        "minute": 1, "minutes": 1,
        "h": 60,
        "hour": 60, "hours": 60,
        "d": 1440,
        "day": 1440, "days": 1440,
        "w": 10080,
        "week": 10080, "weeks": 10080,
        "mo": 43200,
        "month": 43200, "months": 43200,
        "y": 525600,
        "year": 525600, "years": 525600
    }

    match = re.match(
        r"^(\d+)\s?(months?|minutes?|weeks?|years?|hours?|days?|mo|[mhdwy])$",
        value,
        re.IGNORECASE
    )

    if not match:
        raise ValueError(f"Invalid time format: '{value}'")

    amount, unit = match.groups()
    unit = unit.lower()
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
        "B": 1_000_000_000,
        "T": 1_000_000_000_000
    }

    match = re.match(r"^(\d+)(k|M|B|T)$", value)
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

def parse_unit(value: str, filter_key: str = None):
    """
    Parse a string representing a value with optional unit.

    Tries to detect and convert the value as one of:
    - storage size (e.g. "10MB" → "10M")
    - quantity (e.g. "100k" → "100000")
    - time (e.g. "2h" → "120", with suffix "m" or "d")
    - file type (e.g. "dir" → "d")
    - plain integer (e.g. "42" → "42")
    - date (e.g. "2025-07-30" → "2025-07-30T00:00:00")

    Args:
        value: The string to parse
        filter_key: The filter context (e.g. "Size", "UID", "DirCount")
                   Used to prioritize storage vs quantity parsing

    Returns:
        tuple[str, str, str | None]: (value, kind, unit)
        - value: the normalized string (e.g. "100000", "10M", "d")
        - kind: one of "storage", "quantity", "time", "file_type", "int"
        - unit:
            - "m" if time is returned in minutes
            - "d" if time is returned in days
            - None for all other kinds
    """
    # Prioritize storage for Size filter, quantity for others
    if filter_key == "Size":
        parsers = [
            ("storage", parse_storage_unit),
            ("quantity", parse_quantity_unit),
            ("time", parse_time_unit),
            ("file_type", parse_file_type),
            ("int", parse_int),
            ("date", parse_date)
        ]
    else:
        parsers = [
            ("quantity", parse_quantity_unit),
            ("storage", parse_storage_unit),
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

def translate_operator(num: int, operator: str):
    """Convert a comparison operator into the corresponding find-style prefix."""
    if operator == "eq":
        return str(num)
    elif operator == "gt":
        return f"+{num}"
    elif operator == "ge":
        return f"+{max(num - 1, 0)}"
    elif operator == "lt":
        return f"-{num}"
    elif operator == "le":
        return f"-{num + 1}"
    else:
        raise ValueError(f"Unsupported operator: {operator}")

def handle_size(parsed_val: str, kind: str, operator: str):
    if kind == "int":
        parsed_val = f"{parsed_val}c"
        kind = "storage"
    num, suffix = split_number_suffix(parsed_val)
    return translate_operator(num, operator) + suffix

def handle_dircount(parsed_val: str, operator: str):
    num = int(parsed_val)
    return translate_operator(num, operator)

def handle_time(parsed_val: str, operator: str):
    num = int(parsed_val)
    return translate_operator(num, operator)

def translate_condition(key: str, operator: str, value: str):
    keys_strings = {"Path", "Name", "IName", "User", "Group"}
    options = {
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
    }

    option_minutes = {
        "LastAccess": "-amin",
        "LastModification": "-mmin",
        "LastChange": "-cmin",
        "CreationDate": "-bmin",
    }

    if key not in options:
        raise ValueError(f"Unknown key '{key}'")

    if key in keys_strings:
        return [options[key], value]

    parsed_val, kind, unit = parse_unit(value, filter_key=key)

    if key in option_minutes and kind == "time" and unit == "m":
        option = option_minutes[key]
    else:
        option = options[key]

    if key == "Type":
        return [option, parsed_val]

    if key == "Size":
        final = handle_size(parsed_val, kind, operator)
        return [option, final]

    elif key == "DirCount":
        final = handle_dircount(parsed_val, operator)
        return [option, final]

    elif kind == "time" and key in options:
        final = handle_time(parsed_val, operator)
        return [option, final]

    if kind == "date":
        return [option, parsed_val]

    if key in ["UID", "GID"]:
        if kind != "int" and kind != "quantity":
            raise ValueError(f"{key} must be a plain integer, got {kind}")
        return [option, parsed_val]

    return [option, value]

def cmd(cmdline: str) -> str:
    """
    Declare a shell command action.

    Named placeholders like ``{paramname}`` are resolved later against the
    ``parameters`` dict of the enclosing Policy or Rule.  The special
    placeholder ``{}`` is reserved for the filesystem entry path, resolved at
    runtime by the C engine.

    Usage in a config file:
        action = cmd("stat {}")
        action = cmd("archive_tool --level {lvl} {}")
        # with parameters={"lvl": 3}
    """
    if not isinstance(cmdline, str):
        raise TypeError(f"cmd() expects a string, got {type(cmdline).__name__}")
    return f"cmd:{cmdline}"

def resolve_cmd_action(action: str, parameters: dict) -> str:
    """
    Resolve named placeholders in a ``cmd:`` action string against parameters
    and protect ``{}`` from wordexp brace expansion.

    This is called by Policy/Rule constructors once both ``action`` and
    ``parameters`` are known.  Non-cmd actions are returned unchanged.

    :raises ValueError: if a ``{name}`` placeholder remains unresolved after
                        substitution (i.e. it was not provided in parameters).
    """
    if not action or not action.startswith("cmd:"):
        return action

    cmdline = action[len("cmd:"):]

    # Substitute named parameters
    for name, value in (parameters or {}).items():
        cmdline = cmdline.replace(f"{{{name}}}", str(value))

    # Protect the path placeholder from wordexp brace expansion
    cmdline = cmdline.replace("{}", "'{}'")

    # Detect any remaining {name} placeholders not covered by parameters
    unresolved = re.findall(r'\{[^}\']+\}', cmdline)
    if unresolved:
        raise ValueError(
            f"cmd action has unresolved placeholders {unresolved}. "
            f"Add them to the 'parameters' dict of the Policy or Rule."
        )

    return f"cmd:{cmdline}"
def normalize_action(action):
    """
    Normalize an action to a prefixed string for the C policy engine.

    Rules:
    - If action is a Python callable, returns "py:<function_name>".
    - If action is a string already containing ':', returns it unchanged
      (e.g. "common:delete", "cmd:my_tool").
    - Otherwise raises TypeError or ValueError.
    """
    if callable(action):
        name = getattr(action, '__name__', None)
        if not name:
            raise ValueError("Cannot determine function name for action")
        return f"py:{name}"
    if isinstance(action, str):
        if ':' in action:
            return action
        raise ValueError(
            f"Action string '{action}' must include a type prefix "
            f"(e.g. 'common:delete', 'cmd:mytool ...')."
        )
    raise TypeError(
        f"Action must be a callable or a prefixed string, "
        f"got {type(action).__name__}"
    )
