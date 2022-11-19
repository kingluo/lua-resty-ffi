# Golang ffi lib

## Build

```bash
cd examples/lib_ffi_go_etcd
go mod download
go build -buildmode=c-shared -o lib_ffi_go_etcd.so main.go
go build -buildmode=c-shared -o libecho.so echo.go
```

## Test

```bash
LD_LIBRARY_PATH=$PWD /opt/nffi/nginx/sbin/nginx -p $PWD -c nginx.conf -g "daemon off;"
curl localhost:10000
```
