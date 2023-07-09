/*
 * Copyright (c) 2022, Jinhua Luo (kingluo) luajit.io@gmail.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <string.h>
#include <pthread.h>

static PyThreadState *mainThreadState = NULL;

static pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    char* module;
    char* func;
    char* cfg;
    void* tq;
} state_t;

static int init(state_t *state)
{
#if PY_VERSION_HEX >= 0x03080000
    int reload_module = 0;
#endif
    int rc = 1;
    char* module = state->module;
    const char* func = state->func;

    char* last = strrchr(module, '/');
    int dirlen;
    char *dir = NULL;
    if (last != NULL) {
        dirlen = strlen(module) - strlen(last);
        dir = strndup(module, dirlen);
        module = last + 1;
    }

    int lastpos = strlen(module) - 1;
    if (module[lastpos] == '?') {
        module[lastpos] = 0;
#if PY_VERSION_HEX >= 0x03080000
        reload_module = 1;
#endif
    }

    PyObject *pName, *pModule, *pFunc;
    PyObject *pArgs, *pValue;

    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    if (dir != NULL) {
        PyObject* sys = PyImport_ImportModule( "sys" );
        PyObject* sys_path = PyObject_GetAttrString( sys, "path" );
        PyObject* folder_path = PyUnicode_FromString( dir );
        PyList_Insert( sys_path, 0, folder_path );
    }

    pName = PyUnicode_DecodeFSDefault(module);
    /* Error checking of pName left out */

#if PY_VERSION_HEX >= 0x03080000
    pModule = PyImport_GetModule(pName);
    if (pModule == NULL) {
#endif
        pModule = PyImport_Import(pName);
#if PY_VERSION_HEX >= 0x03080000
    } else if (reload_module) {
        pModule = PyImport_ReloadModule(pModule);
    }
#endif

    Py_DECREF(pName);

    if (pModule != NULL) {
        pFunc = PyObject_GetAttrString(pModule, func);
        /* pFunc is a new reference */

        if (pFunc && PyCallable_Check(pFunc)) {
            pArgs = PyTuple_New(2);
            PyTuple_SetItem(pArgs, 0, PyLong_FromVoidPtr(state->cfg));
            PyTuple_SetItem(pArgs, 1, PyLong_FromVoidPtr(state->tq));

            pValue = PyObject_CallObject(pFunc, pArgs);
            Py_DECREF(pArgs);
            if (pValue != NULL) {
                rc = PyLong_AsLong(pValue);
                // printf("Result of call: %ld\n", PyLong_AsLong(pValue));
                Py_DECREF(pValue);
                goto end;
            } else {
                Py_DECREF(pFunc);
                Py_DECREF(pModule);
                PyErr_Print();
                fprintf(stderr,"Call failed\n");
                goto end;
            }
        } else {
            if (PyErr_Occurred()) {
                PyErr_Print();
            }
            fprintf(stderr, "Cannot find function \"%s\"\n", func);
        }
        Py_XDECREF(pFunc);
        Py_DECREF(pModule);
    } else {
        PyErr_Print();
        fprintf(stderr, "Failed to load \"%s\"\n", module);
        goto end;
    }

end:
    PyGILState_Release(gstate);
    return rc;
}

int libffi_init(char* cfg, void *tq)
{
    pthread_mutex_lock(&init_lock);

    if (mainThreadState == NULL) {
        Py_Initialize();
        mainThreadState = PyEval_SaveThread();
    }

    state_t state = {0};
    state.tq = tq;

    char* str = cfg;
    state.module = strsep(&str, ",");
    state.func = strsep(&str, ",");
    state.cfg = str;

    int rc = init(&state);

    pthread_mutex_unlock(&init_lock);

    return rc;
}

__attribute__((destructor)) void ffi_python_fini(void) {
    if (mainThreadState) {
        PyEval_RestoreThread(mainThreadState);
        Py_Finalize();
    }
}
