# Mix all nonblocking-ffi lib

## Test

```bash
CLASSPATH=$PWD/lib_nonblocking_ffi_java:$PWD/lib_nonblocking_ffi_java/http2-demo/target/http2-demo-1.0-SNAPSHOT-jar-with-dependencies.jar:$PWD/lib_nonblocking_ffi_java/echo-demo/target/echo-demo-1.0-SNAPSHOT-jar-with-dependencies.jar LD_LIBRARY_PATH=/usr/lib/jvm/java-17-openjdk-amd64/lib/:/usr/lib/jvm/java-11-openjdk-amd64/lib/server:$PWD/lib_nonblocking_ffi_go_etcd:$PWD/lib_nonblocking_ffi_java:$PWD/lib_nonblocking_ffi_python3  PYTHONPATH=$PWD/lib_nonblocking_ffi_python3 /opt/nffi/nginx/sbin/nginx -p $PWD -c nginx.conf -g "daemon off;"
curl localhost:10000
```
