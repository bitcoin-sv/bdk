#include <Python.h>
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <iostream>

#include <version.hpp>    // global and core versions
#include <version_py.hpp>

#include <interpreter_bdk.hpp>

struct module_state {
    PyObject *error;
};

#if PY_MAJOR_VERSION >= 3
#define GETSTATE(m) ((struct module_state*)PyModule_GetState(m))
#else
#define GETSTATE(m) (&_state)
static struct module_state _state;
#endif


static PyObject* wrap_ExecuteScript(PyObject* self, PyObject *args){
    char* scriptptr;
    int concensus(true);
    unsigned int scriptflags(0);
    char* hextxptr; 
    int index(0);
    int64_t amount(0); 

    if(!PyArg_ParseTuple(args,"siIsii", &scriptptr,&concensus, &scriptflags,&hextxptr,&index,&amount))
        return nullptr;

    try{
        const ScriptError ret = bsv::execute(std::string{scriptptr},concensus, scriptflags, std::string{hextxptr},index,amount);
        return Py_BuildValue("i", ret);
    }catch(std::exception& e){
        PyErr_SetString(PyExc_TypeError, e.what());
        return (PyObject*)nullptr;
    }
}

static PyMethodDef ModuleMethods[] = {
    {"ExecuteScript", wrap_ExecuteScript, METH_VARARGS, "Execute a script"},
    {nullptr, nullptr, 0, nullptr},
};

#if PY_MAJOR_VERSION >= 3

static int PyBDK_traverse(PyObject *m, visitproc visit, void *arg) {
    Py_VISIT(GETSTATE(m)->error);
    return 0;
}

static int PyBDK_clear(PyObject *m) {
    Py_CLEAR(GETSTATE(m)->error);
    return 0;
}

static struct PyModuleDef moduledef = {PyModuleDef_HEAD_INIT,
                                       "PyBDK",
                                       nullptr,
                                       sizeof(struct module_state),
                                       ModuleMethods,
                                       nullptr,
                                       PyBDK_traverse,
                                       PyBDK_clear,
                                       nullptr};

#define INITERROR return nullptr

PyMODINIT_FUNC
PyInit_PyBDK(void)

#else
#define INITERROR return

void
initmyextension(void)
#endif
{
#if PY_MAJOR_VERSION >= 3
    PyObject *module = PyModule_Create(&moduledef);
#else
    PyObject *module = Py_InitModule("PyBDK", myextension_methods);
#endif

    if(module == nullptr)
        INITERROR;
    struct module_state *st = GETSTATE(module);

    st->error = PyErr_NewException("PyBDK.Error", nullptr, nullptr);
    if(st->error == nullptr)
    {
        Py_DECREF(module);
        INITERROR;
    }

    /* Version of Bitcoin SV on which BDK has been built */
    PyModule_AddIntConstant(module, "BSV_CLIENT_VERSION_MAJOR", BSV_CLIENT_VERSION_MAJOR);
    PyModule_AddIntConstant(module, "BSV_CLIENT_VERSION_MINOR", BSV_CLIENT_VERSION_MINOR);
    PyModule_AddIntConstant(module, "BSV_CLIENT_VERSION_REVISION", BSV_CLIENT_VERSION_REVISION);
    PyModule_AddStringConstant(module, "BSV_VERSION_STRING", BSV_VERSION_STRING.c_str());

    PyModule_AddStringConstant(module, "BSV_GIT_COMMIT_TAG_OR_BRANCH", BSV_GIT_COMMIT_TAG_OR_BRANCH.c_str());
    PyModule_AddStringConstant(module, "BSV_GIT_COMMIT_HASH", BSV_GIT_COMMIT_HASH.c_str());
    PyModule_AddStringConstant(module, "BSV_GIT_COMMIT_DATETIME", BSV_GIT_COMMIT_DATETIME.c_str());

    /* Version information inherited from BDK core */
    PyModule_AddIntConstant(module, "BDK_VERSION_MAJOR", BDK_VERSION_MAJOR);
    PyModule_AddIntConstant(module, "BDK_VERSION_MINOR", BDK_VERSION_MINOR);
    PyModule_AddIntConstant(module, "BDK_VERSION_PATCH", BDK_VERSION_PATCH);
    PyModule_AddStringConstant(module, "BDK_VERSION_STRING", BDK_VERSION_STRING.c_str());

    PyModule_AddStringConstant(module, "SOURCE_GIT_COMMIT_TAG_OR_BRANCH", SOURCE_GIT_COMMIT_TAG_OR_BRANCH.c_str());
    PyModule_AddStringConstant(module, "SOURCE_GIT_COMMIT_HASH", SOURCE_GIT_COMMIT_HASH.c_str());
    PyModule_AddStringConstant(module, "SOURCE_GIT_COMMIT_DATETIME", SOURCE_GIT_COMMIT_DATETIME.c_str());
    PyModule_AddStringConstant(module, "BDK_BUILD_DATETIME_UTC", BDK_BUILD_DATETIME_UTC.c_str());

    /* Specific version for python binding module */
    PyModule_AddIntConstant(module, "BDK_PYTHON_VERSION_MAJOR", BDK_PYTHON_VERSION_MAJOR);
    PyModule_AddIntConstant(module, "BDK_PYTHON_VERSION_MINOR", BDK_PYTHON_VERSION_MINOR);
    PyModule_AddIntConstant(module, "BDK_PYTHON_VERSION_PATCH", BDK_PYTHON_VERSION_PATCH);
    PyModule_AddStringConstant(module, "BDK_PYTHON_VERSION_STRING", BDK_PYTHON_VERSION_STRING.c_str());
#if PY_MAJOR_VERSION >= 3
    return module;
#endif
}

