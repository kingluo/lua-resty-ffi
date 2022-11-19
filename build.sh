#!/usr/bin/env bash
set -euo pipefail
set -x

. /etc/os-release

export_openresty_variables() {
    export openssl_prefix=/usr/local/openresty/openssl111
    export zlib_prefix=/usr/local/openresty/zlib
    export pcre_prefix=/usr/local/openresty/pcre

    export cc_opt="-DNGX_LUA_ABORT_AT_PANIC -I${zlib_prefix}/include -I${pcre_prefix}/include -I${openssl_prefix}/include"
    export ld_opt="-L${zlib_prefix}/lib -L${pcre_prefix}/lib -L${openssl_prefix}/lib -Wl,-rpath,${zlib_prefix}/lib:${pcre_prefix}/lib:${openssl_prefix}/lib"
}

prepare_centos7() {
    yum -y install centos-release-scl
    yum -y install devtoolset-9 patch wget git make sudo
    source scl_source enable devtoolset-9

    yum-config-manager --add-repo https://openresty.org/package/centos/openresty.repo
    yum -y install openresty-openssl111-devel openresty-pcre-devel openresty-zlib-devel
}

prepare_debian() {
    if [[ $ID == "ubuntu" ]]; then
        echo "deb http://openresty.org/package/ubuntu ${VERSION_CODENAME} main" > /etc/apt/sources.list.d/openresty.list
    fi

    if [[ $ID == "debian" ]]; then
        echo "deb http://openresty.org/package/debian ${VERSION_CODENAME} openresty" > /etc/apt/sources.list.d/openresty.list
    fi

    apt install -y build-essential openresty-openssl111-dev openresty-pcre-dev openresty-zlib-dev
}

if [[ $ID == "centos" ]]; then
    prepare_centos7
else
    prepare_debian
fi

export_openresty_variables

SRC=$PWD
mkdir -p /opt/resty_ffi/src
cd /opt/resty_ffi/src
wget https://openresty.org/download/openresty-1.21.4.1.tar.gz
tar zxf openresty-1.21.4.1.tar.gz

cd openresty-1.21.4.1/bundle/
for f in $SRC/patches/*; do
    patch -p0 < $f
done

cp -a $SRC/ngx_http_lua_ffi.c ngx_lua-0.10.21/src/
cp -a $SRC/ffi.lua lua-resty-core-0.1.23/lib/resty/core/

cd ..
./configure --prefix=/opt/resty_ffi --with-threads --with-cc-opt="$cc_opt" --with-ld-opt="$ld_opt"
make install
