/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <Python.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "robinhood/policyengine/python.h"

/**
 * Convert a single rbh_value to the corresponding Python object.
 *
 * @return a new reference, or NULL on unsupported/error types
 */
static PyObject *
rbh_value_to_pyobject(const struct rbh_value *val)
{
    switch (val->type) {
    case RBH_VT_BOOLEAN:
        return PyBool_FromLong((long)val->boolean);
    case RBH_VT_INT32:
        return PyLong_FromLong((long)val->int32);
    case RBH_VT_UINT32:
        return PyLong_FromUnsignedLong((unsigned long)val->uint32);
    case RBH_VT_INT64:
        return PyLong_FromLongLong((long long)val->int64);
    case RBH_VT_UINT64:
        return PyLong_FromUnsignedLongLong((unsigned long long)val->uint64);
    case RBH_VT_STRING:
        return PyUnicode_FromString(val->string);
    case RBH_VT_NULL:
        Py_RETURN_NONE;
    default:
        return NULL;
    }
}

/**
 * Call a Python function by name from the __main__ module.
 *
 * ctypes releases the GIL before calling C, so we must re-acquire it with
 * PyGILState_Ensure before touching any Python objects, and release it
 * afterwards.
 *
 * The Python function is called as:
 *   func(abs_path, key1=val1, key2=val2, ...)
 * where abs_path is the entry's absolute path and the keyword arguments
 * come from action->params.
 *
 * @param action      the parsed PYTHON action (action->value is the function
 *                    name in python)
 * @param entry       the fsentry to act on
 * @param mi_backend  the mirror backend, used to resolve the absolute path
 *
 * @return            0 on success, -1 on error
 */
int
rbh_pe_python_action(const struct rbh_action *action,
                     struct rbh_fsentry *entry,
                     struct rbh_backend *mi_backend)
{
    /* action->value is either "func" or "module:func". */
    PyObject *module, *func, *posargs, *kwargs, *result;
    char module_name[256] = "__main__";
    const char *value = action->value;
    PyGILState_STATE gstate;
    PyObject *sys_modules;
    const char *func_name;
    const char *colon;
    const char *path;
    int rc = 0;

    if (!value || !*value) {
        fprintf(stderr, "PythonAction | empty function name\n");
        errno = EINVAL;
        return -1;
    }

    /* Split "module:func" if a colon is present. */
    colon = strchr(value, ':');
    if (colon) {
        size_t mod_len = (size_t)(colon - value);

        if (mod_len >= sizeof(module_name)) {
            fprintf(stderr, "PythonAction | module name too long\n");
            errno = EINVAL;
            return -1;
        }
        memcpy(module_name, value, mod_len);
        module_name[mod_len] = '\0';
        func_name = colon + 1;
    } else {
        func_name = value;
    }

    path = fsentry_absolute_path(mi_backend, entry);

    /* ctypes released the GIL before entering C; re-acquire it before
     * touching any Python object. */
    gstate = PyGILState_Ensure();

    /* Look up the module in sys.modules — it was already imported by
     * load_config, so it is guaranteed to be present there. */
    sys_modules = PySys_GetObject("modules");

    module = PyDict_GetItemString(sys_modules, module_name);
    if (!module) {
        fprintf(stderr,
                "PythonAction | module '%s' not found in sys.modules\n",
                module_name);
        PyGILState_Release(gstate);
        errno = EINVAL;
        return -1;
    }

    /* Look up the function by name: equivalent to getattr(module, func_name). */
    func = PyObject_GetAttrString(module, func_name);
    if (!func || !PyCallable_Check(func)) {
        Py_XDECREF(func);
        fprintf(stderr,
                "PythonAction | function '%s' not found in module '%s'\n",
                func_name, module_name);
        PyErr_Clear();
        PyGILState_Release(gstate);
        errno = EINVAL;
        return -1;
    }

    /* Build the positional args tuple: (abs_path,). */
    posargs = Py_BuildValue("(s)", path ? path : "");
    if (!posargs) {
        Py_DECREF(func);
        PyErr_Clear();
        PyGILState_Release(gstate);
        errno = ENOMEM;
        return -1;
    }

    /* Build the kwargs dict from the action parameter map:
     * {"key": value, ...} → passed as keyword arguments to the function. */
    kwargs = PyDict_New();
    if (!kwargs) {
        Py_DECREF(posargs);
        Py_DECREF(func);
        PyErr_Clear();
        PyGILState_Release(gstate);
        errno = ENOMEM;
        return -1;
    }

    if (action->params.count > 0) {
        const struct rbh_value_map *map = &action->params;

        for (size_t i = 0; i < map->count; i++) {
            const struct rbh_value_pair *pair = &map->pairs[i];
            PyObject *pyval;

            /* Convert the C value to a Python object*/
            pyval = rbh_value_to_pyobject(pair->value);
            if (!pyval) {
                fprintf(stderr,
                        "PythonAction | skipping parameter '%s': "
                        "unsupported type\n", pair->key);
                continue;
            }

            /* PyDict_SetItemString increments the refcount of pyval, so we
             * release our own reference immediately after. */
            if (PyDict_SetItemString(kwargs, pair->key, pyval) < 0) {
                Py_DECREF(pyval);
                Py_DECREF(kwargs);
                Py_DECREF(posargs);
                Py_DECREF(func);
                PyErr_Print();
                PyGILState_Release(gstate);
                errno = ENOMEM;
                return -1;
            }

            Py_DECREF(pyval);
        }
    }

    /* Call func(*posargs, **kwargs), i.e. func(abs_path, key=val, ...). */
    result = PyObject_Call(func, posargs, kwargs);
    Py_DECREF(kwargs);
    Py_DECREF(posargs);
    Py_DECREF(func);

    if (!result) {
        PyErr_Print();
        PyGILState_Release(gstate);
        errno = ENOTSUP;
        return -1;
    }

    /* Extract the integer return code if the function returned one. */
    if (PyLong_Check(result))
        rc = (int)PyLong_AsLong(result);

    Py_DECREF(result);
    PyGILState_Release(gstate);

    return rc;
}
