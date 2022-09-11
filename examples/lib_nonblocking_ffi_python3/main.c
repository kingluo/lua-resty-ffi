#define PY_SSIZE_T_CLEAN
#include <Python.h>

void *threadFunc( void *tq )
{
    const char* module = "kafka";
    const char* func = "init";
    PyObject *pName, *pModule, *pFunc;
    PyObject *pArgs, *pValue;

    Py_Initialize();

    PyObject* sys = PyImport_ImportModule( "sys" );
    PyObject* sys_path = PyObject_GetAttrString( sys, "path" );
    PyObject* folder_path = PyUnicode_FromString( "." );
    PyList_Append( sys_path, folder_path );

    pName = PyUnicode_DecodeFSDefault(module);
    /* Error checking of pName left out */

    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (pModule != NULL) {
        pFunc = PyObject_GetAttrString(pModule, func);
        /* pFunc is a new reference */

        if (pFunc && PyCallable_Check(pFunc)) {
            pArgs = PyTuple_New(3);
            pValue = PyLong_FromLong(666);
            if (!pValue) {
                Py_DECREF(pArgs);
                Py_DECREF(pModule);
                fprintf(stderr, "Cannot convert argument\n");
                return NULL;
            }
            /* pValue reference stolen here: */
            PyTuple_SetItem(pArgs, 0, PyLong_FromVoidPtr(NULL));
            PyTuple_SetItem(pArgs, 1, pValue);
            PyTuple_SetItem(pArgs, 2, PyLong_FromVoidPtr(tq));

            pValue = PyObject_CallObject(pFunc, pArgs);
            Py_DECREF(pArgs);
            if (pValue != NULL) {
                printf("Result of call: %ld\n", PyLong_AsLong(pValue));
                Py_DECREF(pValue);
                return NULL;
            } else {
                Py_DECREF(pFunc);
                Py_DECREF(pModule);
                PyErr_Print();
                fprintf(stderr,"Call failed\n");
                return NULL;
            }
        } else {
            if (PyErr_Occurred())
                PyErr_Print();
            fprintf(stderr, "Cannot find function \"%s\"\n", func);
        }
        Py_XDECREF(pFunc);
        Py_DECREF(pModule);
    } else {
        PyErr_Print();
        fprintf(stderr, "Failed to load \"%s\"\n", module);
        return NULL;
    }
    if (Py_FinalizeEx() < 0) {
        return NULL;
    }
    return NULL;
}

int lib_nonblocking_ffi_init(char* cfg, int cfg_len, void *tq)
{
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create( &thread, &attr, &threadFunc, tq);
    return 0;
}
