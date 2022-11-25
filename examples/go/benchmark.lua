local cjson = require("cjson")
local ffi = require("ffi")
local str_byte = string.byte
local bit = require("bit")
local band = bit.band
local rshift = bit.rshift
local ffi_str = ffi.string

local _M = {}

function _M.run(n, arr)
    ngx.say("loop times=", n, ", lengths=", cjson.encode(arr))
    ngx.flush(true)

    for _, blen in ipairs(arr) do
        local buf = string.rep("A", blen)
        local buflen = #buf
        assert(buflen==blen)
        ngx.say("-- benchmark len=", buflen)

        local demo = ngx.load_ffi("ffi_go_echo")
        ngx.update_time()
        local t1 = ngx.now()
        for _ = 1,n do
            local ok, res = demo:echo(buf)
            assert(ok)
            assert(res==buf)
        end
        ngx.update_time()
        local t2 = ngx.now()
        ngx.say("ffi: ", t2-t1)
        ngx.flush(true)

        -- unix domain socket
        local sock = ngx.socket.tcp()
        local ok, err = sock:connect("unix:/tmp/echo.sock")
        assert(ok)

        local tbuf = {}
        local hdr_buf = ffi.new("unsigned char[4]")

        ngx.update_time()
        local t1 = ngx.now()
        for _ = 1,n do
            local len = buflen
            for i = 3, 0, -1 do
                hdr_buf[i] = band(len, 255)
                len = rshift(len, 8)
            end

            tbuf[1] = ffi_str(hdr_buf, 4)
            tbuf[2] = buf
            local sent = sock:send(tbuf)
            assert(sent==buflen+4)

            local hdr = sock:receive(4)
            local hi, mi, li, lli = str_byte(hdr, 1, 4)
            local len = 256 * (256 * (256 * hi + mi) + li) + lli
            assert(len==buflen)
            local data = sock:receive(len)
            assert(data==buf)
        end
        ngx.update_time()
        local t2 = ngx.now()
        ngx.say("uds: ", t2-t1)
        sock:close()
        ngx.flush(true)
    end
end

return _M
