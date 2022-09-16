# lua-resty-nonblocking-ffi

## Env

`Ubuntu 20.04`

## Build

```bash
apt install -y build-essential

cd /opt
git clone https://github.com/kingluo/lua-resty-nonblocking-ffi
wget https://openresty.org/download/openresty-1.21.4.1.tar.gz
tar zxf openresty-1.21.4.1.tar.gz

# patch and install files
cd /opt/openresty-1.21.4.1/bundle/ngx_lua-0.10.21/
patch -p1 < /opt/lua-resty-nonblocking-ffi/patches/config.patch
cd /opt/openresty-1.21.4.1/bundle/lua-resty-core-0.1.23/lib/resty/
patch -p1 < /opt/lua-resty-nonblocking-ffi/patches/core.lua.patch
cd /opt/openresty-1.21.4.1/bundle/nginx-1.21.4/src/core/
patch -p1 < /opt/lua-resty-nonblocking-ffi/patches/ngx_thread_pool.c.patch

cd /opt/lua-resty-nonblocking-ffi/
cp -a ngx_http_lua_nonblocking_ffi.c ngx_http_lua_nonblocking_ffi.h /opt/openresty-1.21.4.1/bundle/ngx_lua-0.10.21/src/
cp -a nonblocking_ffi.lua /opt/openresty-1.21.4.1/bundle/lua-resty-core-0.1.23/lib/resty/core/

cd /opt/openresty-1.21.4.1

./configure --prefix=/opt/nffi --with-threads
make install
```
