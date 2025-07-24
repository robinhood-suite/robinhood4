# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import ctypes
import ctypes.util
from ctypes import c_int, c_char_p, c_void_p, Structure, POINTER, Union

librbh = ctypes.CDLL("librobinhood.so")

libc = ctypes.CDLL(ctypes.util.find_library('c'))
libc.free.argtypes = [c_void_p]
libc.free.restype = None

# robinhood-specific extensions for statx
RBH_STATX_TYPE         = 0x00000001
RBH_STATX_UID          = 0x00000008
RBH_STATX_GID          = 0x00000010
RBH_STATX_ATIME_SEC    = 0x00000020
RBH_STATX_MTIME_SEC    = 0x00000040
RBH_STATX_CTIME_SEC    = 0x00000080
RBH_STATX_SIZE         = 0x00000200
RBH_STATX_BTIME_SEC    = 0x00000800

# Enums fsentry_property
RBH_FP_NAME             = 0x0004
RBH_FP_STATX            = 0x0008
RBH_FP_SYMLINK          = 0x0010
RBH_FP_NAMESPACE_XATTRS = 0x0020

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

FIELD_MAPPING = {
    "Name": {
        "fsentry": RBH_FP_NAME,
        "type": "string"
    },
    "IName": {
        "fsentry": RBH_FP_NAME,
        "type": "string"
    },
    "Path": {
        "fsentry": RBH_FP_NAMESPACE_XATTRS,
        "xattr": b"path",
        "type": "string"
    },
    "UID": {
        "fsentry": RBH_FP_STATX,
        "statx": RBH_STATX_UID,
        "type": "uint"
    },
    "GID": {
        "fsentry": RBH_FP_STATX,
        "statx": RBH_STATX_GID,
        "type": "uint"
    },
    "Size": {
        "fsentry": RBH_FP_STATX,
        "statx": RBH_STATX_SIZE,
        "type": "string"
    },
    "LastAccess": {
        "fsentry": RBH_FP_STATX,
        "statx": RBH_STATX_ATIME_SEC,
        "type": "int"
    },
    "LastModification": {
        "fsentry": RBH_FP_STATX,
        "statx": RBH_STATX_MTIME_SEC,
        "type": "int"
    },
    "LastChange": {
        "fsentry": RBH_FP_STATX,
        "statx": RBH_STATX_CTIME_SEC,
        "type": "int"
    },
    "CreationDate": {
        "fsentry": RBH_FP_STATX,
        "statx": RBH_STATX_BTIME_SEC,
        "type": "int"
    },
    "User": {
        "fsentry": RBH_FP_STATX,
        "statx": RBH_STATX_UID,
        "type": "string"
    },
    "Group": {
        "fsentry": RBH_FP_STATX,
        "statx": RBH_STATX_GID,
        "type": "string"
    },
    "Type": {
        "fsentry": RBH_FP_STATX,
        "statx": RBH_STATX_TYPE,
        "type": "string"
    },
}

class RbhFilterFieldUnion(Union):
    _fields_ = [
        ("statx", ctypes.c_uint32),
        ("xattr", ctypes.c_char_p)
    ]

class RbhFilterField (Structure):
    _anonymous_ = ("union",)
    _fields_ = [
        ("fsentry", c_int),
        ("union", RbhFilterFieldUnion)
    ]

# struct rbh_filter* (opaque to avoid mapping)
rbh_filter_p = c_void_p

librbh.rbh_filter_compare_string_new.restype = rbh_filter_p
librbh.rbh_filter_compare_string_new.argtypes = [c_int,
        POINTER(RbhFilterField), c_char_p]

librbh.rbh_filter_and.restype = rbh_filter_p
librbh.rbh_filter_and.argtypes = [rbh_filter_p, rbh_filter_p]

librbh.rbh_filter_or.restype = rbh_filter_p
librbh.rbh_filter_or.argtypes = [rbh_filter_p, rbh_filter_p]

librbh.rbh_filter_not.restype = rbh_filter_p
librbh.rbh_filter_not.argtypes = [rbh_filter_p]

librbh.rbh_filter_compare_uint64_new.restype = rbh_filter_p
librbh.rbh_filter_compare_uint64_new.argtypes = [c_int,
        POINTER(RbhFilterField), ctypes.c_uint64]

librbh.rbh_filter_compare_int64_new.restype = rbh_filter_p
librbh.rbh_filter_compare_int64_new.argtypes = [c_int,
        POINTER(RbhFilterField), ctypes.c_int64]

librbh.rbh_filter_validate.restype = ctypes.c_int
librbh.rbh_filter_validate.argtypes = [rbh_filter_p]


def rbh_filter_compare_new(operator, field_name, value):
    if field_name not in FIELD_MAPPING:
        raise ValueError(f"Unknown field: {field_name}")

    info = FIELD_MAPPING[field_name]

    field = RbhFilterField()
    field.fsentry = info["fsentry"]

    if "statx" in info:
        field.statx = info["statx"]
    elif "xattr" in info:
        field.xattr = info["xattr"]
    else:
        field.xattr = None

    if info["type"] == "string":
        return librbh.rbh_filter_compare_string_new(
            operator, ctypes.byref(field), str(value).encode()
        )
    elif info["type"] == "uint":
        return librbh.rbh_filter_compare_uint64_new(operator,
                ctypes.byref(field), value)
    elif info["type"] == "int":
        return librbh.rbh_filter_compare_int64_new(operator,
                ctypes.byref(field), value)
    else:
        raise ValueError(f"Unsupported field type: {info['type']}")

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
