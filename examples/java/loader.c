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

#define _GNU_SOURCE
#include <stdio.h>
#include <jni.h>
#include <string.h>
#include <stdlib.h>

static JavaVM *vm;
static JNIEnv *env;

int libffi_init(char* cfg, void *tq)
{
    if (vm == NULL) {
        JavaVMInitArgs  vm_args;
        jint            res;
        jclass          cls;
        jmethodID       mid;
        jstring         jstr;
        jobjectArray    main_args;

        #define MAX_OPTS 50
        JavaVMOption options[MAX_OPTS];
        vm_args.version  = JNI_VERSION_1_8;
        vm_args.nOptions = 0;
        vm_args.options = options;

        char* path = NULL;
        char* class_path = getenv("CLASSPATH");
        if (class_path) {
            if (asprintf(&path, "-Djava.class.path=%s", class_path) == -1) {
                return 1;
            }
            options[vm_args.nOptions++].optionString = path;
        }

        char* java_opts = getenv("JAVA_OPTS");
        char* opts = NULL;
        if (java_opts) {
            opts = strdup(java_opts);
            char* opt;
            while ((opt = strsep(&opts, " "))) {
                printf("opt: %s\n", opt);
                options[vm_args.nOptions++].optionString = opt;
                if (vm_args.nOptions == MAX_OPTS) {
                    break;
                }
            }
        }

        res = JNI_CreateJavaVM(&vm, (void **)&env, &vm_args);
        if (path) {
            free(path);
        }
        if (opts) {
            free(opts);
        }
        if (res != JNI_OK) {
            printf("Failed to create Java VM\n");
            return 1;
        }
    }

    char* str = cfg;
    char* class = strsep(&str, ",");
    for (char* tmp = class; *tmp != 0; tmp++) {
        if (*tmp == '.') {
            *tmp = '/';
        }
    }
    char* method = strsep(&str, ",");
    char* cfg_str= str;

    jclass          cls;
    jmethodID       mid;

    cls = (*env)->FindClass(env, class);
    if (cls == NULL) {
        printf("Failed to find Main class\n");
        return 1;
    }

    mid = (*env)->GetStaticMethodID(env, cls, method, "(Ljava/lang/String;J)I");
    if (mid == NULL) {
        printf("Failed to find main function\n");
        return 1;
    }

    jstring jcfg = (*env)->NewStringUTF(env, cfg_str);
    int rc = (*env)->CallStaticIntMethod(env, cls, mid, jcfg, (jlong)tq);
    (*env)->DeleteLocalRef(env, jcfg);

    /* Check for errors. */
    if ((*env)->ExceptionOccurred(env)) {
        (*env)->ExceptionDescribe(env);
    }

    return rc;
}
