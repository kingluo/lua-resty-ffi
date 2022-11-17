from cffi import FFI
ffi = FFI()
ffi.cdef("""
char *strdup(const char *s);
void* ngx_http_lua_nonblocking_ffi_task_poll(void *p);
char* ngx_http_lua_nonblocking_ffi_get_req(void *tsk, int *len);
void ngx_http_lua_nonblocking_ffi_respond(void *tsk, int rc, char* rsp, int rsp_len);
""")
C = ffi.dlopen(None)

import threading

class State:
    def poll(self, tq):
        while True:
            task = C.ngx_http_lua_nonblocking_ffi_task_poll(ffi.cast("void*", tq))
            if task == ffi.NULL:
                break
            req = C.ngx_http_lua_nonblocking_ffi_get_req(task, ffi.NULL)
            res = C.strdup(req)
            C.ngx_http_lua_nonblocking_ffi_respond(task, 0, res, 0)
        print("exit python echo runtime")

def init(cfg, tq):
    st = State()
    t = threading.Thread(target=st.poll, args=(tq,))
    t.daemon = True
    t.start()
    return 0
