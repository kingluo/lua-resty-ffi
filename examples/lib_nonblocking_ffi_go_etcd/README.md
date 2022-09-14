# Golang nonblocking-ffi lib

## Build

```bash
cd examples/lib_nonblocking_ffi_python3
go mod download
CGO_CFLAGS="-I/opt/lua-resty-nonblocking-ffi" go build -buildmode=c-shared -o lib_nonblocking_ffi_go_etcd.so main.go
```

## Test

```bash
LD_LIBRARY_PATH=$PWD /opt/nffi/nginx/sbin/nginx -p $PWD -c nginx.conf -g "daemon off;"
curl localhost:10000
```
