package = "lua-resty-ffi"
version = "main-1"
source = {
    url = "git+https://github.com/kingluo/lua-resty-ffi",
    branch = "main",
}

description = {
    summary = "efficient and generic API to do hybrid programming in openresty",
    detailed = [[
    With lua-resty-ffi, You could develop openresty libraries in mainstream languages (Go, Python, Java, Rust, Node.js).

    Features:

    * nonblocking, in coroutine way

    * simple but extensible interface, supports any C ABI compliant language

    * once and for all, no need to write C/Lua codes to do coupling anymore

    * high performance, faster than unix domain socket way

    * generic loader library for python/java/nodejs

    * any serialization message format you like
    ]],
    homepage = "https://github.com/kingluo/lua-resty-ffi",
    license = " BSD-3-Clause",
    maintainer = "Jinhua Luo <luajit.io@gmail.com>",
}

dependencies = {
}

build = {
    type = "make",
    makefile = "Makefile.rocks",
    build_variables = {
        CFLAGS="$(CFLAGS)",
        LIBFLAG="$(LIBFLAG)",
        LUA_LIBDIR="$(LUA_LIBDIR)",
        LUA_BINDIR="$(LUA_BINDIR)",
        LUA_INCDIR="$(LUA_INCDIR)",
        LUA="$(LUA)",
        OR_SRC="$(OR_SRC)",
        NGINX_BIN="$(NGINX_BIN)",
    },
    install_variables = {
        INST_PREFIX="$(PREFIX)",
        INST_BINDIR="$(BINDIR)",
        INST_LIBDIR="$(LIBDIR)",
        INST_LUADIR="$(LUADIR)",
        INST_CONFDIR="$(CONFDIR)",
    },
}
