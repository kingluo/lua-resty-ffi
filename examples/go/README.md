# Golang ffi library examples

* `echo.go`

Echo your request.

* `etcd.go`

Simple etcd client, implements put and watch operations.

## Install golang if not yet

https://go.dev/doc/install

```bash
wget https://go.dev/dl/go1.19.3.linux-amd64.tar.gz
rm -rf /usr/local/go && tar -C /usr/local -xzf go1.19.3.linux-amd64.tar.gz
export PATH=$PATH:/usr/local/go/bin
```

## Build and test

```bash
# in one terminal

# install latest docker version
# https://docs.docker.com/engine/install/

# start etcd container for test
docker compose up -d

# compile example libraries
make

# run nginx
make run
# or specify nginx executable file path
# make run NGINX_BIN=/path/to/nginx

# in another terminal
# curl the tests
make test
```

## Library Skelton

```go
//export libffi_init
func libffi_init(cfg_cstr *C.char, tq unsafe.Pointer) C.int {
    go func() {
        for {
            // poll a task
            task := C.ngx_http_lua_ffi_task_poll(tq)
            // if task queue is done, then quit
            if task == nil {
                break
            }
            // get the request message from the task
            var rlen C.int
            r := C.ngx_http_lua_ffi_get_req(task, &rlen)
            
            // copy the request as response, allocated by C malloc()
            // note that both request and response would be freed by nginx
            res := C.malloc(C.ulong(rlen))
            C.memcpy(res, unsafe.Pointer(r), C.ulong(rlen))
            C.ngx_http_lua_ffi_respond(task, 0, (*C.char)(res), rlen)
        }
    }()
    return 0
}
```

