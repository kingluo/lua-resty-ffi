#!/usr/bin/env bash
set -euo pipefail
set -x

NGINX_BIN=${NGINX_BIN:-/opt/resty_ffi/nginx/sbin/nginx}

. /etc/os-release

[[ -d logs ]] || mkdir logs

export CLASSPATH=${PWD}/http2-demo/target/http2-demo-1.0-SNAPSHOT-jar-with-dependencies.jar
CLASSPATH+=:${PWD}/echo-demo/target/echo-demo-1.0-SNAPSHOT-jar-with-dependencies.jar
CLASSPATH+=:${PWD}/loader/target/resty-ffi-loader-1.0-SNAPSHOT-jar-with-dependencies.jar

if [[ $ID == "centos" ]]; then
    export LD_LIBRARY_PATH=/usr/lib/jvm/java-11-openjdk/lib/server:/usr/lib/jvm/java-11-openjdk/lib:/usr/local/lib/lua/5.1:${PWD}
else
    export LD_LIBRARY_PATH=/usr/lib/jvm/java-11-openjdk-amd64/lib/server:/usr/lib/jvm/java-11-openjdk-amd64/lib:/usr/local/lib/lua/5.1:${PWD}
fi

${NGINX_BIN} -p ${PWD} -c nginx.conf
