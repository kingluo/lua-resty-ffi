#!/usr/bin/env bash
set -euo pipefail
set -x

. /etc/os-release

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

if [[ $ID == "centos" ]]; then
    prepare_centos
    yum -y install python3-devel
    gcc $(python3-config --cflags) $(python3-config --ldflags) -o libresty_ffi_python.so loader.c \
        -fPIC -shared -pthread
else
    apt -y install build-essential
    apt -y install python3-dev python3-pip libffi-dev
    if [[ $ID == "ubuntu" ]]; then
        gcc $(python3-config --cflags) $(python3-config --ldflags) -o libresty_ffi_python.so loader.c \
            -lpython$(python3 -c 'import sys; print(".".join(map(str, sys.version_info[:2])))') \
            -fPIC -shared -pthread
    elif [[ $ID == "debian" ]]; then
        gcc $(python3-config --cflags) $(python3-config --ldflags) -o libresty_ffi_python.so loader.c \
            -fPIC -shared -pthread
    else
        echo "$ID not supported"
        exit 1
    fi
fi
