#include <jni.h>
#include <string.h>
#include <stdlib.h>

static JavaVM *vm;
static JNIEnv *env;

char* nth_strchr(const char* s, int c, int n)
{
    int c_count;
    char* nth_ptr;

    for (c_count=1, nth_ptr = strchr(s, c);
            nth_ptr != NULL && c_count < n && c != 0;
            c_count++)
    {
        nth_ptr = strchr(nth_ptr+1, c);
    }

    return nth_ptr;
}

int lib_nonblocking_ffi_init(char* cfg, void *tq)
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

    char* sep1 = nth_strchr(cfg, (int)',', 1);
    char* class = strndup(cfg, sep1-cfg);
    char* sep2 = nth_strchr(cfg, (int)',', 2);
    char* method = strndup(sep1+1, sep2-sep1-1);
    char* cfg_str = sep2 + 1;

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
