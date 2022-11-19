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

        JavaVMOption options[1];
        vm_args.version  = JNI_VERSION_1_8;
        vm_args.nOptions = 0;
        vm_args.options = options;

        char* class_path = getenv("CLASSPATH");
        if (class_path) {
            char path[4096];
            sprintf(path, "-Djava.class.path=%s", class_path);
            options[vm_args.nOptions++].optionString = path;
        }

        res = JNI_CreateJavaVM(&vm, (void **)&env, &vm_args);
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
