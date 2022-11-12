local cjson = require("cjson")
local grpc = ngx.load_nonblocking_ffi("grpc_client")

local NEW_CONNECTION = 0
local CLOSE_CONNECTION = 1
local UNARY = 2
local NEW_STREAM = 3
local CLOSE_STREAM = 4
local CLOSE_SEND = 5
local STREAM_SEND = 6
local STREAM_RECV = 7

local _M = {}

local objs = {}

ngx.timer.every(3, function()
    if #objs > 0 then
        for _, s in ipairs(objs) do
            local ok = s:close()
            assert(ok)
            print(s.conn, ", ", s.stream)
        end
        objs = {}
    end
end)

local pb = {
    _info = {},
    _pb = require("pb"),
    _pc = require("protoc").new(),
    _collect = function(self, name)
        for _, s in ipairs(self._pc.loaded[name].service or {}) do
            for _, m in ipairs(s.method) do
                self._info["/" .. self._pc.loaded[name].package .. '.' .. s.name .. "/" .. m.name] = m
            end
        end
    end,
    load = function(self, src)
        self._pc:load(src, "<nil>")
        self:_collect("<nil>")
    end,
    loadfile = function(self, name)
        self._pc:loadfile(name)
        self:_collect(name)
    end,
    encode = function(self, path, req)
        return self._pb.encode(self._info[path].input_type, req)
    end,
    decode = function(self, path, res)
        return self._pb.decode(self._info[path].output_type, res)
    end,
}

function setmt__gc(t, mt)
    local prox = newproxy(true)
    getmetatable(prox).__gc = function() mt.__gc(t) end
    t[prox] = true
    return setmetatable(t, mt)
end

local stream_meta = {
    __gc = function(self)
        if self.closed then
            return
        end
        table.insert(objs, self)
    end,
    __index = {
        send = function(self, req)
            return grpc:call(cjson.encode({
                cmd = STREAM_SEND,
                key = self.stream,
                payload = ngx.encode_base64(pb:encode(self.path, req)),
            }))
        end,
        close_send = function(self)
            return grpc:call(cjson.encode({
                cmd = CLOSE_SEND,
                key = self.stream,
            }))
        end,
        recv = function(self)
            local ok, res = grpc:call(cjson.encode({
                cmd = STREAM_RECV,
                key = self.stream,
            }))
            return ok, (ok and res) and pb:decode(self.path, res) or nil
        end,
        close = function(self)
            self.closed = true
            return grpc:call(cjson.encode({cmd = CLOSE_STREAM, key = self.stream}))
        end,
    },
}

local meta = {
    __gc = function(self)
        if self.closed then
            return
        end
        table.insert(objs, self)
    end,
    __index = {
        unary = function(self, path, req)
            local ok, res = grpc:call(cjson.encode({
                cmd = UNARY,
                key = self.conn,
                path = path,
                payload = ngx.encode_base64(pb:encode(path, req)),
            }))
            assert(ok)
            return ok, ok and pb:decode(path, res) or nil
        end,
        new_stream = function(self, path)
            local ok, res = grpc:call(cjson.encode({
                cmd = NEW_STREAM,
                key = self.conn,
                path = path,
            }))
            return ok, ok and setmt__gc({
                stream = res,
                path = path,
                closed = false,
            }, stream_meta) or nil
        end,
        close = function(self)
            return grpc:call(cjson.encode({cmd = CLOSE_CONNECTION, key = self.conn}))
        end,
    }
}

function _M.loadfile(file)
    return pb:loadfile(file)
end

function _M.connect(uri, opts)
    opts = opts or {}
    local cmd = {
        cmd = NEW_CONNECTION,
        key = uri,
        ca = opts.ca,
        host = opts.host,
        cert = opts.cert,
        priv_key = opts.priv_key,
    }
    local ok, conn = grpc:connect(cjson.encode(cmd))
    return ok, ok and setmt__gc({conn = conn, closed = false}, meta) or nil
end

function _M.readfile(file)
    local f = assert(io.open(file, "r"))
    local content = f:read("*all")
    f:close()
    return content
end

return _M
