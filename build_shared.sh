#!/usr/bin/env bash
set -euo pipefail
set -x

prepare_centos() {
    if [[ $(rpm --eval '%{centos_ver}') == "7" ]]; then
        yum -y install centos-release-scl
        yum -y install devtoolset-9 patch wget git make sudo
        set +eu
        source scl_source enable devtoolset-9
        set -eu
    elif [[ $(rpm --eval '%{centos_ver}') == "8" ]]; then
        dnf install -y gcc-toolset-9-toolchain patch wget git make sudo
        dnf install -y yum-utils
        set +eu
        source /opt/rh/gcc-toolset-9/enable
        set -eu
    fi
}

symbol_addr() {
    nm $NGINX_BIN | awk '{if($3=="'"$1"'")print "0x" $1}'
}

generate_offsets() {
    addr0=$(symbol_addr ngx_cycle)
    printf "#define NGX_THREAD_POOL_DONE_OFFSET 0x%X\n" $(($(symbol_addr ngx_thread_pool_done)-$addr0))
    printf "#define NGX_THREAD_POOL_DONE_LOCK_OFFSET 0x%X\n" $(($(symbol_addr ngx_thread_pool_done_lock)-$addr0))
    addr0=$(symbol_addr ngx_calloc)
    printf "#define NGX_THREAD_POOL_HANDLER_OFFSET 0x%X\n" $(($(symbol_addr ngx_thread_pool_handler)-$addr0))
}

. /etc/os-release

if [[ $ID == "centos" ]]; then
    prepare_centos
fi

NGX_BUILD=$(find $OR_SRC/build -maxdepth 1 -name 'nginx-*' -type d)
if [[ -z "$NGX_BUILD" ]]; then
    echo 'You need to configure $OR_SRC first.'
    exit 1
fi
NGINX_BIN="${NGINX_BIN:-$NGX_BUILD/objs/nginx}"
if [[ ! -x "$NGINX_BIN" ]]; then
    echo 'You need to specify $NGINX_BIN.'
    exit 1
fi
generate_offsets > symbols.h

BUILD_ID=$(file $NGINX_BIN | sed -En 's/.*BuildID\[sha1\]=([a-z0-9]+).*/\1/p')
if [[ -z "$BUILD_ID" ]]; then
    echo "$NGINX_BIN has no build-id."
    exit 1
fi
echo "#define NGX_BUILD_ID \"$BUILD_ID\"" >> symbols.h

export NGX_LUA_SRC="$(find $OR_SRC/build/ -maxdepth 1 -name 'ngx_lua-*' -type d)/src"
export NGX_OBJS_MAKEFILE="$NGX_BUILD/objs/Makefile"

export SRC=$PWD
(cd $NGX_BUILD; make -f $SRC/Makefile)
