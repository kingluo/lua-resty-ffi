# JAVA ffi lib

## Build

```bash
cd examples/lib_ffi_java
apt install openjdk-17-jdk
gcc -shared -fPIC -o lib_ffi_java.so main.c -I../../ -I/usr/lib/jvm/java-17-openjdk-amd64/include/ -I/usr/lib/jvm/java-17-openjdk-amd64/include/linux/ -L/usr/lib/jvm/java-17-openjdk-amd64/lib/ -L/usr/lib/jvm/java-17-openjdk-amd64/lib/server/ -ljli -ljvm -lpthread
(cd http2-demo; mvn -DskipTests=true -Dmaven.test.skip=true package)
(cd echo-demo; mvn -DskipTests=true -Dmaven.test.skip=true package)
```

## Test

```bash
CLASSPATH=$PWD:$PWD/http2-demo/target/http2-demo-1.0-SNAPSHOT-jar-with-dependencies.jar:$PWD/echo-demo/target/echo-demo-1.0-SNAPSHOT-jar-with-dependencies.jar LD_LIBRARY_PATH=/usr/lib/jvm/java-17-openjdk-amd64/lib/:/usr/lib/jvm/java-11-openjdk-amd64/lib/server:$PWD /opt/nffi/nginx/sbin/nginx -p $PWD -c nginx.conf -g "daemon off;"

curl localhost:10000

:status: 200
access-control-allow-credentials: true
access-control-allow-origin: *
server: gunicorn/19.9.0
content-length: 375
date: Thu, 15 Sep 2022 15:50:24 GMT
content-type: application/json
{
  "args": {},
  "data": "foobar",
  "files": {},
  "form": {},
  "headers": {
    "Content-Length": "6",
    "Host": "httpbin.org",
    "User-Agent": "Java-http-client/11.0.14.1",
    "X-Amzn-Trace-Id": "Root=1-632349c0-652f912715f22bba007881ed"
  },
  "json": null,
  "method": "GET",
  "origin": "xxx",
  "url": "https://httpbin.org/anything/get"
}

---------
hello

```
