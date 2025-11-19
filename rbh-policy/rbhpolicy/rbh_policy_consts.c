#include <Python.h>
#include "robinhood/filter.h"

static struct PyModuleDef consts_module = {
    PyModuleDef_HEAD_INIT,
    "rbh_policy_consts",
    "Robinhood filter operator constants",
    -1,
    NULL, NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC PyInit_rbh_policy_consts(void)
{
    PyObject *mod = PyModule_Create(&consts_module);
    if (!mod)
        return NULL;

    PyModule_AddIntMacro(mod, RBH_FOP_EQUAL);
    PyModule_AddIntMacro(mod, RBH_FOP_STRICTLY_LOWER);
    PyModule_AddIntMacro(mod, RBH_FOP_LOWER_OR_EQUAL);
    PyModule_AddIntMacro(mod, RBH_FOP_STRICTLY_GREATER);
    PyModule_AddIntMacro(mod, RBH_FOP_GREATER_OR_EQUAL);
    PyModule_AddIntMacro(mod, RBH_FOP_REGEX);
    PyModule_AddIntMacro(mod, RBH_FOP_AND);
    PyModule_AddIntMacro(mod, RBH_FOP_OR);
    PyModule_AddIntMacro(mod, RBH_FOP_NOT);

    return mod;
}
