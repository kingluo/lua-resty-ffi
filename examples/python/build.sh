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
    gcc $(python3-config --cflags) $(python3-config --ldflags) -o libffi_python3.so loader.c \
        -fPIC -shared -pthread
elif [[ $ID == "ubuntu" ]]; then
    gcc $(python3-config --cflags) $(python3-config --ldflags) -o libffi_python3.so loader.c \
        -lpython$(python3 -c 'import sys; print(".".join(map(str, sys.version_info[:2])))') \
        -fPIC -shared -pthread
elif [[ $ID == "debian" ]]; then
    gcc $(python3-config --cflags) $(python3-config --ldflags) -o libffi_python3.so loader.c \
        -fPIC -shared -pthread
else
    echo "$ID not supported"
    exit 1
fi

pip3 install -r requirements.txt
