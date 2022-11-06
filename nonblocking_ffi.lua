local ffi = require "ffi"
local C = ffi.C
local base = require "resty.core.base"
local get_request = base.get_request

local runtimes = {}

ffi.cdef[[
void *malloc(size_t size);
int lib_nonblocking_ffi_init(char* cfg, void *tq);
void* ngx_nonblocking_ffi_create_task_queue(int max_queue);
int ngx_http_lua_nonblocking_ffi_task_post(void *r, void* tq, char* req, int req_len);
int ngx_http_lua_nonblocking_ffi_task_finish(void *p);
]]

local function post(self, req)
    if self.finished then
        return false, "task queue is finished"
    end
    local buf = nil
    local buf_len = 0
    if req then
        buf_len = #req
        buf = C.malloc(#req + 1)
        ffi.copy(buf, req)
    end
    local ret = C.ngx_http_lua_nonblocking_ffi_task_post(get_request(), self.tq, buf, buf_len)
    if ret ~= 0 then
        return false, "post failed, queue full"
    end
    return coroutine._yield()
end

local function unload(self)
    if not self.finished then
        self.finished = true
        self.handle = nil
        C.ngx_http_lua_nonblocking_ffi_task_finish(self.tq)
        self.tq = nil
        runtimes[self.key] = nil
        self.key = nil
    end
end

local mt = {
    __index = function(self, k)
        rawset(self, k, post)
        return post
    end,
    __call = post,
}

ngx.load_nonblocking_ffi = function(lib, cfg, opts)
    local max_queue = 65536
    local is_global = false
    if opts ~= nil then
        if opts.max_queue ~= nil then
            max_queue = opts.max_queue
        end
        if opts.is_global ~= nil then
            is_global = opts.is_global
        end
    end

    local key = lib
    if cfg then
        key = lib .. "&" .. cfg
    end
    if runtimes[key] then
        return runtimes[key]
    end

    local tq = C.ngx_nonblocking_ffi_create_task_queue(max_queue)
    local nffi = setmetatable({
        finished = false,
        key = key,
        handle = ffi.load(lib, is_global),
        tq = tq,
        __unload = unload,
    }, mt)

    local buf = nil
    if cfg then
        buf = C.malloc(#cfg + 1)
        ffi.copy(buf, cfg)
    end
    local rc = nffi.handle.lib_nonblocking_ffi_init(buf, tq)
    if rc == 0 then
        runtimes[key] = nffi
        return nffi
    end
    return nil, rc
end
