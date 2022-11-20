#!/usr/bin/env bash
set -euo pipefail
set -x

NGINX=${NGINX:-/opt/resty_ffi/nginx/sbin/nginx}

. /etc/os-release

[[ -d logs ]] || mkdir logs

if [[ $ID == "centos" ]]; then
	CLASSPATH=${PWD}/http2-demo/target/http2-demo-1.0-SNAPSHOT-jar-with-dependencies.jar:${PWD}/echo-demo/target/echo-demo-1.0-SNAPSHOT-jar-with-dependencies.jar LD_LIBRARY_PATH=/usr/lib/jvm/java-11-openjdk/lib/server:/usr/lib/jvm/java-11-openjdk/lib:${PWD} ${NGINX} -p ${PWD} -c nginx.conf
else
	CLASSPATH=${PWD}/http2-demo/target/http2-demo-1.0-SNAPSHOT-jar-with-dependencies.jar:${PWD}/echo-demo/target/echo-demo-1.0-SNAPSHOT-jar-with-dependencies.jar LD_LIBRARY_PATH=/usr/lib/jvm/java-11-openjdk-amd64/lib/server:/usr/lib/jvm/java-11-openjdk-amd64/lib:${PWD} ${NGINX} -p ${PWD} -c nginx.conf
fi
