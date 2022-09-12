local ffi = require "ffi"
local C = ffi.C
local base = require "resty.core.base"
local get_request = base.get_request

local libs = {}

ffi.cdef[[
void *malloc(size_t size);
int lib_nonblocking_ffi_init(char* cfg, int cfg_len, void *tq);
void* ngx_nonblocking_ffi_create_task_queue(int max_queue);
int ngx_http_lua_nonblocking_ffi_task_post(ngx_http_request_t *r,
    void* tq, char* req, int req_len);
]]

local function post(self, req)
    local buf = nil
    local buf_len = 0
    if req then
        buf_len = #req
        buf = C.malloc(#req)
        ffi.copy(buf, req)
    end
    local ret = C.ngx_http_lua_nonblocking_ffi_task_post(get_request(), self.tq, buf, buf_len)
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

ngx.load_nonblocking_ffi = function(lib, cfg, max_queue, is_global)
    local key = lib
    if cfg then
        key = lib .. "&" .. cfg
    end
    if libs[key] then
        return libs[key]
    end

    if not max_queue then
        max_queue = 65536
    end

    local tq = C.ngx_nonblocking_ffi_create_task_queue(max_queue)
    local nffi = setmetatable({
        handle = ffi.load(lib, is_global),
        tq = tq,
    }, mt)

    local buf = nil
    local buf_len = 0
    if cfg then
        buf_len = #cfg
        buf = C.malloc(#cfg)
        ffi.copy(buf, cfg)
    end
    local rc = nffi.handle.lib_nonblocking_ffi_init(buf, buf_len, tq)
    if rc == 0 then
        libs[key] = nffi
        return nffi
    end
end
