#!/usr/bin/env bash
set -euo pipefail
set -x

. /etc/os-release

if [[ $ID == "centos" ]]; then
    set +eu
    if [[ $VERSION_ID == "7" ]]; then
        source scl_source enable devtoolset-9
    elif [[ $VERSION_ID == "8" ]]; then
        source /opt/rh/gcc-toolset-9/enable
    fi
    set -eu
fi

g++ -std=c++17 -pthread -I/usr/local/include/node/ \
    -fPIC -shared -lnode -L/usr/local/lib -pthread \
    -o libffi_nodejs.so loader.cc

node-gyp configure build
