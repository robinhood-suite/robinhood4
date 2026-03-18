# This file is part of RobinHood
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

import yaml
import ctypes
import ctypes.util
from ctypes import (
    c_int,
    c_uint,
    c_bool,
    c_char_p,
    c_void_p,
    Structure,
    POINTER,
    Union,
)

librbh = ctypes.CDLL("librobinhood.so")

libc = ctypes.CDLL(ctypes.util.find_library('c'))
libc.free.argtypes = [c_void_p]
libc.free.restype = None

# struct rbh_filter* (opaque to avoid mapping)
rbh_mut_iterator_p = c_void_p
rbh_backend_p = c_void_p
rbh_filter_p = c_void_p

librbh.rbh_filter_and.restype = rbh_filter_p
librbh.rbh_filter_and.argtypes = [rbh_filter_p, rbh_filter_p]

librbh.rbh_filter_or.restype = rbh_filter_p
librbh.rbh_filter_or.argtypes = [rbh_filter_p, rbh_filter_p]

librbh.rbh_filter_not.restype = rbh_filter_p
librbh.rbh_filter_not.argtypes = [rbh_filter_p]

librbh.rbh_filter_validate.restype = ctypes.c_int
librbh.rbh_filter_validate.argtypes = [rbh_filter_p]

librbh.build_filter_from_uri.restype = rbh_filter_p
librbh.build_filter_from_uri.argtypes = [c_char_p, ctypes.POINTER(c_char_p)]

librbh.rbh_backend_from_uri.restype = rbh_backend_p
librbh.rbh_backend_from_uri.argtypes = [c_char_p, c_bool]

librbh.rbh_backend_get_source_backend.restype = c_char_p
librbh.rbh_backend_get_source_backend.argtypes = [c_void_p]

librbh.rbh_backend_get_mountpoint.restype = c_char_p
librbh.rbh_backend_get_mountpoint.argtypes = [c_void_p]

librbh.rbh_collect_fsentries.restype = rbh_mut_iterator_p
librbh.rbh_collect_fsentries.argtypes = [rbh_backend_p, rbh_filter_p]

librbh.rbh_pe_execute.restype = c_int
librbh.rbh_pe_execute.argtypes = [rbh_mut_iterator_p, rbh_backend_p, c_char_p,
                                  c_void_p]

librbh.rbh_trigger_query_stat.restype = c_int
librbh.rbh_trigger_query_stat.argtypes = [rbh_backend_p, rbh_filter_p,
                                          ctypes.c_uint32,
                                          ctypes.POINTER(ctypes.c_int64)]

class RbhDeleteParams(Structure):
    _fields_ = [
        ("remove_empty_parent", c_bool),
        ("remove_parents_below", c_char_p),
    ]

class RbhLogParams(Structure):
    _fields_ = [
        ("format", c_char_p),
    ]

class RbhParamsUnion(Union):
    _fields_ = [
        ("generic", c_char_p),
        ("delete", RbhDeleteParams),
        ("log", RbhLogParams),
    ]

class RbhRule(Structure):
    _fields_ = [
        ("name", c_char_p),
        ("filter", rbh_filter_p),
        ("action", c_char_p),
        ("parameters", RbhParamsUnion),
    ]

class RbhPolicy(Structure):
    _fields_ = [
        ("name", c_char_p),
        ("filter", rbh_filter_p),
        ("action", c_char_p),
        ("parameters", RbhParamsUnion),
        ("rules", POINTER(RbhRule)),
        ("rule_count", ctypes.c_size_t),
    ]

def make_c_parameters(action: str, params: dict):
    action = action or ""
    verb = action.split(":", 1)[-1]  # "_:delete" -> "delete"

    if verb == "delete":
        p = RbhDeleteParams()
        p.remove_empty_parent = bool(params.get("remove_empty_parent", False))

        below = params.get("remove_parents_below")
        p.remove_parents_below = below.encode() if below else None

        return ("delete", p)

    elif verb == "log":
        p = RbhLogParams()
        log_format = params.get("format")
        p.format = log_format.encode() if log_format else None
        return ("log", p)

    yaml_bytes = yaml.safe_dump(params or {}).encode()
    return ("generic", yaml_bytes)

def make_c_rules(py_rules):
    arr = (RbhRule * len(py_rules))()
    for i, rule in enumerate(py_rules):
        kind, value = make_c_parameters(rule.action, rule.parameters)
        arr[i].name = rule.name.encode()
        arr[i].filter = rule.to_filter()
        arr[i].action = (rule.action or "").encode()

        if kind == "delete":
            arr[i].parameters.delete = value
        elif kind == "log":
            arr[i].parameters.log = value
        else:
            arr[i].parameters.generic = value
    return arr, len(py_rules)

def make_c_policy(py_policy):
    rules_arr, rules_len = make_c_rules(py_policy.rules)
    kind, value = make_c_parameters(py_policy.action, py_policy.parameters)
    c_policy = RbhPolicy()
    c_policy.name = py_policy.name.encode()
    c_policy.filter = py_policy.to_filter()
    c_policy.action = (py_policy.action or "").encode()

    if kind == "delete":
        c_policy.parameters.delete = value
    elif kind == "log":
        c_policy.parameters.log = value
    else:
        c_policy.parameters.generic = value

    c_policy.rules = rules_arr
    c_policy.rule_count = rules_len

    return c_policy

def build_filter(args):
    global backend # The URI, comes from the configuration file.
    if backend is None:
        raise RuntimeError("The 'backend' variable (the URI) is not "
                           "initialized.")

    if not isinstance(args, list) or len(args) != 2:
        raise ValueError("The function expects a list of two strings: "
                         "[predicate, value]")

    c_args = [arg.encode() for arg in args] + [None]
    argv = (c_char_p * len(c_args))(*c_args)

    return librbh.build_filter_from_uri(backend.encode(), argv)

def collect_fs_entries(rbhfilter):
    global database
    uri_c = c_char_p(database.encode("utf-8"))
    backend = librbh.rbh_backend_from_uri(uri_c, True)
    if not backend:
        raise RuntimeError("Failed to create backend")

    it = librbh.rbh_collect_fsentries(backend, rbhfilter)

    return it, backend

def rbh_pe_execute(mirror_iter, mirror_backend, policy_obj):
    global backend
    c_policy = make_c_policy(policy_obj)
    fs_uri_c = backend.encode('utf-8')

    result = librbh.rbh_pe_execute(mirror_iter, mirror_backend, fs_uri_c,
                                   ctypes.byref(c_policy))

    return result

def trigger_query_stat(rbh_filter, stat_field):
    """
    Query aggregated statistics from the database backend.

    @param rbh_filter   pre-built filter from build_filter(); NULL queries all
    @param stat_field   0 for count, RBH_STATX_SIZE, etc.

    @return             aggregated value (count or sum)
    """
    global database
    uri_c = c_char_p(database.encode("utf-8"))
    backend = librbh.rbh_backend_from_uri(uri_c, True)
    if not backend:
        raise RuntimeError("Failed to create database backend")

    result = ctypes.c_int64()
    rc = librbh.rbh_trigger_query_stat(backend, rbh_filter,
                                       ctypes.c_uint32(stat_field),
                                       ctypes.byref(result))

    if rc != 0:
        raise RuntimeError(f"rbh_trigger_query_stat failed: {rc}")

    return result.value

def rbh_filter_and(filter1, filter2):
    return librbh.rbh_filter_and(filter1, filter2)

def rbh_filter_or(filter1, filter2):
    return librbh.rbh_filter_or(filter1, filter2)

def rbh_filter_not(filter1):
    return librbh.rbh_filter_not(filter1)

def rbh_filter_validate(filter1):
    return librbh.rbh_filter_validate(filter1)

def rbh_filter_free(ptr):
    if ptr:
        libc.free(ptr)

def set_backend(uri: str):
    global backend
    backend = uri

def resolve_backend_info(uri: str, getter) -> str:
    if not uri:
        return None

    uri_c = c_char_p(uri.encode("utf-8"))
    backend_ptr = librbh.rbh_backend_from_uri(uri_c, True)
    if not backend_ptr:
        return None

    raw = getter(backend_ptr)
    if not raw:
        return None

    return raw.decode()

def resolve_backend_type(uri: str) -> str:
    return resolve_backend_info(uri, librbh.rbh_backend_get_source_backend)

def resolve_backend_mountpoint(uri: str) -> str:
    return resolve_backend_info(uri, librbh.rbh_backend_get_mountpoint)

def set_database(uri: str):
    global database
    database = uri

def get_database():
    global database
    return database

def set_evaluation_interval(interval):
    global evaluation_interval
    evaluation_interval = interval

# Initialize global policy engine settings used by admin config scripts.
# Sets `backend`, `database` and `evaluation_interval` variables.
def config(*, filesystem, database, evaluation_interval):
    """Initialize backend, database and evaluation interval."""
    from rbhpolicy.config.config_validator import validate_config
    validate_config(filesystem, database, evaluation_interval)
    set_backend(filesystem)
    set_database(database)
    set_evaluation_interval(evaluation_interval)
