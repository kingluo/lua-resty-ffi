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

    yum-config-manager --add-repo https://openresty.org/package/centos/openresty.repo
    yum -y install openresty-openssl111-devel openresty-pcre-devel openresty-zlib-devel
}

prepare_debian() {
    DEBIAN_FRONTEND=noninteractive apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y sudo git perl build-essential curl
    DEBIAN_FRONTEND=noninteractive apt-get -y install --no-install-recommends wget gnupg ca-certificates

    wget -O - https://openresty.org/package/pubkey.gpg | apt-key add -

    if [[ $ID == "ubuntu" ]]; then
        echo "deb http://openresty.org/package/ubuntu ${VERSION_CODENAME} main" > /etc/apt/sources.list.d/openresty.list
    fi

    if [[ $ID == "debian" ]]; then
        echo "deb http://openresty.org/package/debian ${VERSION_CODENAME} openresty" > /etc/apt/sources.list.d/openresty.list
    fi

    apt update

    apt install -y build-essential openresty-openssl111-dev openresty-pcre-dev openresty-zlib-dev
}

if [[ $ID == "centos" ]]; then
    prepare_centos
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
./configure --prefix=/opt/resty_ffi --with-threads --with-cc-opt="$cc_opt" --with-ld-opt="$ld_opt" $@
make install

# compile and install luarocks for luajit and openssl from openresty
cd /tmp
wget https://github.com/luarocks/luarocks/archive/v3.8.0.tar.gz
tar zxf v3.8.0.tar.gz
cd luarocks-3.8.0
./configure --with-lua=/usr/local/openresty/luajit
make install
export PATH=$PATH:/usr/local/bin
mkdir ~/.luarocks
luarocks config variables.OPENSSL_LIBDIR /usr/local/openresty/openssl111/lib
luarocks config variables.OPENSSL_INCDIR /usr/local/openresty/openssl111/include
