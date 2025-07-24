# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import ctypes
import ctypes.util
from ctypes import c_int, c_uint, c_char_p, c_void_p, Structure, POINTER, Union

librbh = ctypes.CDLL("librobinhood.so")

libc = ctypes.CDLL(ctypes.util.find_library('c'))
libc.free.argtypes = [c_void_p]
libc.free.restype = None

backend: str = None

# Enums filter_operator
RBH_FOP_EQUAL            = 0
RBH_FOP_STRICTLY_LOWER   = 1
RBH_FOP_LOWER_OR_EQUAL   = 2
RBH_FOP_STRICTLY_GREATER = 3
RBH_FOP_GREATER_OR_EQUAL = 4
RBH_FOP_REGEX            = 5

RBH_FOP_AND              = 12
RBH_FOP_OR               = 13
RBH_FOP_NOT              = 14

# WIP : Some difficulty with the project architecture to implement this.
"""
from rbhpolicy import rbh_policy_consts
RBH_FOP_EQUAL = rbh_policy_consts.RBH_FOP_EQUAL
RBH_FOP_STRICTLY_LOWER = rbh_policy_consts.RBH_FOP_STRICTLY_LOWER
RBH_FOP_LOWER_OR_EQUAL = rbh_policy_consts.RBH_FOP_LOWER_OR_EQUAL
RBH_FOP_STRICTLY_GREATER = rbh_policy_consts.RBH_FOP_STRICTLY_GREATER
RBH_FOP_GREATER_OR_EQUAL = rbh_policy_consts.RBH_FOP_GREATER_OR_EQUAL
RBH_FOP_REGEX = rbh_policy_consts.RBH_FOP_REGEX

RBH_FOP_AND = rbh_policy_consts.RBH_FOP_AND
RBH_FOP_OR = rbh_policy_consts.RBH_FOP_OR
RBH_FOP_NOT = rbh_policy_consts.RBH_FOP_NOT
"""

# struct rbh_filter* (opaque to avoid mapping)
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
