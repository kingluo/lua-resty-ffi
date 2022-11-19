#!/usr/bin/env bash
set -euo pipefail
set -x

. /etc/os-release

if [[ $ID == "centos" ]]; then
    yum install java-11-openjdk-devel
    gcc -O2 -shared -fPIC -o libffi_java.so loader.c -I/usr/lib/jvm/java-11-openjdk/include/ -I/usr/lib/jvm/java-11-openjdk/include/linux/ -L/usr/lib/jvm/java-11-openjdk/lib/ -L/usr/lib/jvm/java-11-openjdk/lib/server/ -ljvm -pthread
else
	apt -y install openjdk-11-jdk
	gcc -O2 -shared -fPIC -o libffi_java.so loader.c -I/usr/lib/jvm/java-11-openjdk-amd64/include/ -I/usr/lib/jvm/java-11-openjdk-amd64/include/linux/ -L/usr/lib/jvm/java-11-openjdk-amd64/lib/ -L/usr/lib/jvm/java-11-openjdk-amd64/lib/server/ -ljvm -pthread
fi

(cd http2-demo; mvn -q -DskipTests=true -Dmaven.test.skip=true package)
(cd echo-demo; mvn -q -DskipTests=true -Dmaven.test.skip=true package)
