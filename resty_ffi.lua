--
-- Copyright (c) 2022, Jinhua Luo (kingluo) luajit.io@gmail.com
-- All rights reserved.
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions are met:
--
-- 1. Redistributions of source code must retain the above copyright notice, this
--    list of conditions and the following disclaimer.
--
-- 2. Redistributions in binary form must reproduce the above copyright notice,
--    this list of conditions and the following disclaimer in the documentation
--    and/or other materials provided with the distribution.
--
-- 3. Neither the name of the copyright holder nor the names of its
--    contributors may be used to endorse or promote products derived from
--    this software without specific prior written permission.
--
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
-- DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
-- FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
-- DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
-- SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
-- CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
-- OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
-- OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--
local ffi = require "ffi"
local C = ffi.C
local get_request
local ok, base = pcall(require, "resty.core.base")
if ok then
    get_request = base.get_request
end

local init = false
local function test_symbol(sym) return C[sym] ~= nil end
local resty_ffi = C

local runtimes = {}

ffi.cdef[[
void *malloc(size_t size);
void free(void *ptr);
int libffi_init(char* cfg, void *tq);
void* ngx_http_lua_ffi_create_task_queue(int max_queue);
int ngx_http_lua_ffi_task_post(void *r, void* tq, char* req, int req_len);
int ngx_http_lua_ffi_task_finish(void *p);
int lua_resty_ffi_init();
]]

local function post(self, req, wrapper)
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
    local r = wrapper and wrapper:this() or get_request()
    local ret = resty_ffi.ngx_http_lua_ffi_task_post(r, self.tq, buf, buf_len)
    if ret ~= 0 then
        return false, "post failed, queue full"
    end
    if wrapper then
        return wrapper:ffiYield()
    end
    return coroutine._yield()
end

local function unload(self)
    if not self.finished then
        self.finished = true
        self.handle = nil
        resty_ffi.ngx_http_lua_ffi_task_finish(self.tq)
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

local pin_libs = {}

local function ffi_load(lib, is_global, is_pin)
    local handle = ffi.load(lib, is_global)
    if handle and is_pin then
        pin_libs[lib] = handle
    end
    return handle
end

ngx = ngx or {}
ngx.load_ffi = function(lib, cfg, opts)
    if not init then
        if not pcall(test_symbol, "ngx_http_lua_ffi_create_task_queue") then
            local handle = ffi.load("resty_ffi", true)
            if handle.lua_resty_ffi_init() ~= 0 then
                error("lua_resty_ffi_init() failed: nginx build-id not found or mismatch")
            end
            resty_ffi = handle
        end
        init = true
    end

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

    local is_pin = (not opts) or (not opts.unpin)
    local tq = resty_ffi.ngx_http_lua_ffi_create_task_queue(max_queue)
    local nffi = setmetatable({
        finished = false,
        key = key,
        handle = ffi_load(lib, is_global, is_pin),
        tq = tq,
        __unload = unload,
    }, mt)

    local buf = nil
    if cfg then
        buf = C.malloc(#cfg + 1)
        ffi.copy(buf, cfg)
    end
    local rc = nffi.handle.libffi_init(buf, tq)
    if buf then
        C.free(buf)
    end
    if rc == 0 then
        runtimes[key] = nffi
        return nffi
    end
    return nil, rc
end
