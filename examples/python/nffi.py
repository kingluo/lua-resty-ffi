from cffi import FFI
ffi = FFI()
ffi.cdef("""
void* malloc(size_t);
void *memcpy(void *dest, const void *src, size_t n);
char *strdup(const char *s);
void* ngx_http_lua_nonblocking_ffi_task_poll(void *p);
char* ngx_http_lua_nonblocking_ffi_get_req(void *tsk, int *len);
void ngx_http_lua_nonblocking_ffi_respond(void *tsk, int rc, char* rsp, int rsp_len);
""")
C = ffi.dlopen(None)
