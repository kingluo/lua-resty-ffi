from nffi import *

def init(cfg, cfg_len, tq):
    while True:
        task = C.ngx_http_lua_nonblocking_ffi_task_poll(ffi.cast("void*", tq))
        req = C.ngx_http_lua_nonblocking_ffi_get_req(task, ffi.NULL)
        res = C.strdup(req)
        C.ngx_http_lua_nonblocking_ffi_respond(task, 0, res, 0)
    return 0
