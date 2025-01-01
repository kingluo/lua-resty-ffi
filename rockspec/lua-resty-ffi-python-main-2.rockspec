package = "lua-resty-ffi-python"
version = "main-2"
source = {
    url = "git+https://github.com/kingluo/lua-resty-ffi",
    branch = "main",
}

description = {
    summary = "lua-resty-ffi python loader library",
    homepage = "https://github.com/kingluo/lua-resty-ffi",
    license = " BSD-3-Clause",
    maintainer = "Jinhua Luo <luajit.io@gmail.com>",
}

dependencies = {
    "lua-resty-ffi = main-2",
}

build = {
    type = "make",
    makefile = "Makefile-python.rocks",
    build_variables = {
        CFLAGS="$(CFLAGS)",
        LIBFLAG="$(LIBFLAG)",
        LUA_LIBDIR="$(LUA_LIBDIR)",
    },
    install_variables = {
        INST_PREFIX="$(PREFIX)",
        INST_LIBDIR="$(LIBDIR)",
    },
}
