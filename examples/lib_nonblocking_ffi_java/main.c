#include <jni.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
extern void* ngx_http_lua_nonblocking_ffi_task_poll(void *p);
extern char* ngx_http_lua_nonblocking_ffi_get_req(void *tsk, int *len);
extern void ngx_http_lua_nonblocking_ffi_respond(void *tsk, int rc, char* rsp, int rsp_len);

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
            printf("Failed to create Java VMn");
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

    mid = (*env)->GetStaticMethodID(env, cls, method, "(JLjava/lang/String;)I");
    if (mid == NULL) {
        printf("Failed to find main function\n");
        return 1;
    }

    jstring jcfg = (*env)->NewStringUTF(env, cfg_str);
    int rc = (*env)->CallStaticIntMethod(env, cls, mid, (jlong)tq, jcfg);
    (*env)->DeleteLocalRef(env, jcfg);

    /* Check for errors. */
    if ((*env)->ExceptionOccurred(env)) {
        (*env)->ExceptionDescribe(env);
    }

    return rc;
}

static const char *JNIT_CLASS = "resty/NgxHttpLuaNonblockingFFI";

static jlong c_nonblocking_ffi_task_poll(JNIEnv *env, jobject obj, jlong p)
{
    jlong ret = (jlong) ngx_http_lua_nonblocking_ffi_task_poll((void*) p);
    return ret;
}

static jbyteArray c_nonblocking_ffi_get_req(JNIEnv *env, jobject obj, jlong p)
{
    int len;
    char* req = ngx_http_lua_nonblocking_ffi_get_req((void*) p, &len);
    jbyteArray bytes = (*env)->NewByteArray(env, len);
    (*env)->SetByteArrayRegion(env, bytes, 0, len, (jbyte*) req);
    return bytes;
}

static void c_nonblocking_ffi_respond(JNIEnv *env, jobject obj, jlong p, jint rc, jbyteArray barr)
{
    char* rtn = NULL;
    jsize alen = (*env)->GetArrayLength(env, barr);
    jbyte* ba = (*env)->GetByteArrayElements(env, barr, JNI_FALSE);
    if (alen > 0) {
        rtn = (char*) malloc(alen);
        memcpy(rtn, ba, alen);
        rtn[alen] = 0;
    }
    ngx_http_lua_nonblocking_ffi_respond((void*) p, rc, rtn, alen);
}

static JNINativeMethod funcs[] = {
    { "task_poll", "(J)J", (void *)&c_nonblocking_ffi_task_poll },
    { "get_req", "(J)[B", (void *)&c_nonblocking_ffi_get_req },
    { "respond", "(JI[B)V", (void *)&c_nonblocking_ffi_respond },
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv *env;
    jclass  cls;
    jint    res;

    (void)reserved;

    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_8) != JNI_OK)
        return -1;

    cls = (*env)->FindClass(env, JNIT_CLASS);
    if (cls == NULL)
        return -1;

    res = (*env)->RegisterNatives(env, cls, funcs, sizeof(funcs)/sizeof(*funcs));
    if (res != 0)
        return -1;

    return JNI_VERSION_1_8;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved)
{
    JNIEnv *env;
    jclass  cls;

    (void)reserved;

    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_8) != JNI_OK)
        return;

    cls = (*env)->FindClass(env, JNIT_CLASS);
    if (cls == NULL)
        return;

    (*env)->UnregisterNatives(env, cls);
}
