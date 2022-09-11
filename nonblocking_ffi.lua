local ffi = require "ffi"
local C = ffi.C
local base = require "resty.core.base"
local get_request = base.get_request

local libs = {}

ffi.cdef[[
void *malloc(size_t size);
void lib_nonblocking_ffi_init(void *tq);
void* ngx_nonblocking_ffi_create_task_queue(int max_queue);
int ngx_http_lua_nonblocking_ffi_task_post(ngx_http_request_t *r,
    void* tq, char* req, int req_len);
]]

local function post(self, req)
    local buf = C.malloc(#req)
    ffi.copy(buf, req)
    local ret = C.ngx_http_lua_nonblocking_ffi_task_post(get_request(), self.tq, buf, #req)
    if ret ~= 0 then
        return false, "post failed, queue full"
    end
    return coroutine._yield()
end

local mt = {
    __index = function(self, k)
        rawset(self, k, post)
        return post
    end,
    __call = post,
}

ngx.load_nonblocking_ffi = function(lib, max_queue)
    if libs[lib] then
        return libs[lib]
    end

    if not max_queue then
        max_queue = 65536
    end

    local tq = C.ngx_nonblocking_ffi_create_task_queue(max_queue)
    local nffi = setmetatable({
        handle = ffi.load(lib),
        tq = tq,
    }, mt)
    nffi.handle.lib_nonblocking_ffi_init(tq)
    libs[lib] = nffi
    return nffi
end
