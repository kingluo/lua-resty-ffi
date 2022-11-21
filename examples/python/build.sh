#!/usr/bin/env bash
set -euo pipefail
set -x

. /etc/os-release

if [[ $ID == "centos" ]]; then
    if [[ $VERSION_ID == "7" ]]; then
        source scl_source enable devtoolset-9
    elif [[ $VERSION_ID == "8" ]]; then
        source /opt/rh/gcc-toolset-9/enable
    fi
    yum -y install python3-devel
    gcc $(python3-config --cflags) $(python3-config --ldflags) -o libffi_python3.so loader.c \
        -fPIC -shared -pthread
elif [[ $ID == "ubuntu" ]]; then
    apt -y install python3-dev python3-pip libffi-dev
    gcc $(python3-config --cflags) $(python3-config --ldflags) -o libffi_python3.so loader.c \
        -lpython$(python3 -c 'import sys; print(".".join(map(str, sys.version_info[:2])))') \
        -fPIC -shared -pthread
elif [[ $ID == "debian" ]]; then
    apt -y install python3-dev python3-pip libffi-dev
    gcc $(python3-config --cflags) $(python3-config --ldflags) -o libffi_python3.so loader.c \
        -fPIC -shared -pthread
else
    echo "$ID not supported"
    exit 1
fi

pip3 install -r requirements.txt
