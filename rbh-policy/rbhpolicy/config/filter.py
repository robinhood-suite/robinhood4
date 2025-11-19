# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import json
import ctypes
import ctypes.util
from ctypes import c_int, c_uint, c_char_p, c_void_p, Structure, POINTER, Union

librbh = ctypes.CDLL("librobinhood.so")

libc = ctypes.CDLL(ctypes.util.find_library('c'))
libc.free.argtypes = [c_void_p]
libc.free.restype = None

# struct rbh_filter* (opaque to avoid mapping)
rbh_filter_p = c_void_p
rbh_mut_iterator_p = c_void_p

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

librbh.rbh_collect_fsentry.restype = rbh_mut_iterator_p
librbh.rbh_collect_fsentry.argtypes = [c_char_p, rbh_filter_p]

class RbhRule(Structure):
    _fields_ = [
        ("name", c_char_p),
        ("filter", rbh_filter_p),
        ("action", c_char_p),
        ("parameters", c_char_p),  # JSON string
    ]

class RbhPolicy(Structure):
    _fields_ = [
        ("name", c_char_p),
        ("action", c_char_p),
        ("parameters", c_char_p),
        ("rules", POINTER(RbhRule)),
        ("rule_count", ctypes.c_size_t),
    ]

def make_c_rules(py_rules):
    arr = (RbhRule * len(py_rules))()
    for i, rule in enumerate(py_rules):
        params_json = json.dumps(rule.parameters or {}).encode()
        arr[i].name = rule.name.encode()
        arr[i].filter = rule.to_filter()
        arr[i].action = (rule.action or "").encode()
        arr[i].parameters = params_json
    return arr, len(py_rules)

def make_c_policy(py_policy):
    rules_arr, rules_len = make_c_rules(py_policy.rules)
    root_params_json = json.dumps(py_policy.parameters or {}).encode()
    c_policy = RbhPolicy()
    c_policy.name = py_policy.name.encode()
    c_policy.action = (py_policy.action or "").encode()
    c_policy.parameters = root_params_json
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
    return librbh.rbh_collect_fsentry(uri_c, rbhfilter)

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

def set_database(uri: str):
    global database
    database = uri
