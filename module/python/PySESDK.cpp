#include <Python.h>
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <iostream>

#include <ScriptEngineIF.h>

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
    int nIndex(0);
    int64_t amount(0); 

    if(!PyArg_ParseTuple(args,"siIsii", &scriptptr,&concensus, &scriptflags,&hextxptr,&nIndex,&amount))
        return NULL;

    
    try{
        const ScriptError ret = bsv::evaluate(std::string{scriptptr},concensus, scriptflags, std::string{hextxptr},nIndex,amount);
        return Py_BuildValue("i", ret);
    }catch(std::exception& e){
        PyErr_SetString(PyExc_TypeError, e.what());
        return(PyObject *) NULL;
    }
}

static PyMethodDef ModuleMethods[] =
{
    {"ExecuteScript", wrap_ExecuteScript,METH_VARARGS,"Execute a script"},
    {NULL, NULL, 0, NULL},
};
 
#if PY_MAJOR_VERSION >= 3

static int PySESDK_traverse(PyObject *m, visitproc visit, void *arg) {
    Py_VISIT(GETSTATE(m)->error);
    return 0;
}

static int PySESDK_clear(PyObject *m) {
    Py_CLEAR(GETSTATE(m)->error);
    return 0;
}


static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "PySESDK",
        NULL,
        sizeof(struct module_state),
        ModuleMethods,
        NULL,
        PySESDK_traverse,
        PySESDK_clear,
        NULL
};

#define INITERROR return NULL

PyMODINIT_FUNC
PyInit_PySESDK(void)

#else
#define INITERROR return

void
initmyextension(void)
#endif
{
#if PY_MAJOR_VERSION >= 3
    PyObject *module = PyModule_Create(&moduledef);
#else
    PyObject *module = Py_InitModule("PySESDK", myextension_methods);
#endif

    if (module == NULL)
        INITERROR;
    struct module_state *st = GETSTATE(module);

    st->error = PyErr_NewException("PySESDK.Error", NULL, NULL);
    if (st->error == NULL) {
        Py_DECREF(module);
        INITERROR;
    }

#if PY_MAJOR_VERSION >= 3
    return module;
#endif
}

