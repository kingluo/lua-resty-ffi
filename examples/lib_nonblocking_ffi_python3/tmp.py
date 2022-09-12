import threading
from cffi import FFI
ffi = FFI()
ffi.cdef("""
void* malloc(size_t);
void *memcpy(void *dest, const void *src, size_t n);
void* ngx_http_lua_nonblocking_ffi_task_poll(void *p);
char* ngx_http_lua_nonblocking_ffi_get_req(void *tsk, int *len);
void ngx_http_lua_nonblocking_ffi_respond(void *tsk, int rc, char* rsp, int rsp_len);
""")
C = ffi.dlopen(None)

def init(cfg, cfg_len, tq):
    while True:
        task = C.ngx_http_lua_nonblocking_ffi_task_poll(ffi.cast("void*", tq))
        res = C.malloc(3)
        C.memcpy(res, b"tmp", 3)
        C.ngx_http_lua_nonblocking_ffi_respond(task, 0, res, 3)
    return 0
