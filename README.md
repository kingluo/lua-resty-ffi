# lua-resty-nonblocking-ffi

lua-resty-nonblocking-ffi provides an efficient and generic API to do hybrid programming in openresty with mainstream languages (Go, Python, Java, Rust, etc.).

**Features:**
* simple but extensible interface, supports any C ABI compliant language
* once and for all, no need to write C/Lua codes to do coupling anymore
* high performance, 3~4 times faster than unix domain socket way
* shim for python/java, once and for all
* any serialization message format you like

## Background

In openresty land, when you turn to implement some logic, especially to couple with third-party popular frameworks, it's likely to suck in awkward: make bricks without straw.

1. C is a low-level language, very little ecosystem, no unified and rich libraries, and almost all modern frameworks do not support C, instead, they like Java, Python, Go. For example, when you need to do grpc to access external services, you must depend on C++ lib, which is huge and cumbersome.

2. Lua is an embedded and minority programming language, which means all the powers comes from the host. In openresty, it means all functionalities come from lua-nginx-modules. Like C, or even worse, you have to reinvent the wheels via cosocket to do modern networking stuff. A lot of lua-resty-* were born, but they are almost semi-finished compared to native lib in other languages. For example, lua-resty-kafka doesn't support consumer groups, lua-resty-postgres doesn't support notify and prepared statements, etc. Moreover, most of those authors of lua-resty-* stop development at some stage because the lua community is so small and less attractive.

To implement a common function in other mainstream languages, you have to do a lot of adaptive coding back and forward between nginx and openresty. For example, http2 requires SNI and viable session reuse, then you have to patch C codes and openresty and/or nginx. Moreover, when you turn to accomplish a new job, you need to redo all such things again! Then, a lot of *-nginx-module born and recompile your nginx! Painful, right?

**Why not WASM?**

WASM lacks of below features, which is useless esepcially for openresty:
* no coroutine, which means you need to execute the logic from start to end and block the nginx worker process with arbitrary time
* castrated language support, e.g. go doesn't suppport WASM fully, you need to use tiny-go
* WASI is not supported in almost all lanuages, which means it's hard to use their battery-included lib and ecosystem
* complex development, due to sandbox original intention, you have to export a lot of API for callbacks
* threading model is pending feature and almost all languages do not support it

All in all, WASM is painful and hard way to adapt other languages into the Nginx world.

**May I extend the openresty with modern programming languages (Go, Python, Java, etc.) and reuse their rich ecosystems directly? That means, we reserved everything from those mainstream languages and let them work together with Nginx at ease.**

This library has been used successfully by several commercial
companies for over two years. My customers come from different
domains, including AI, IOT, Edge computing.

## Architecture

![1663476422225](https://user-images.githubusercontent.com/4401042/190886217-9fb97d6c-bf3f-435d-bdd8-702ce86dde2d.png)

## Concepts

### Library

In Go, it means the output library generated by `go build` e.g. `lib_nonblocking_ffi_go_etcd.so`.

In Python3, it means the shim library `lib_nonblocking_ffi_python3.so`, and native python3 modules.

In Java, it means the shim library `lib_nonblocking_ffi_java.so`, and native Java classes/jar.

### Library configuration

Configuration of the library, e.g. etcd endpoints, kafka endpoints, etc.

The format could be any serialization format, e.g. json, yaml, as long as it matches the runtime 

### Runtime

The combination of library and configuration would init a new runtime, which may include some threads or goroutines to do jobs.

You could use the same library with different configuration, which is a common case.

### Request-Response Model

Coupling between nginx worker process and the runtime is based on message exchanges, which contains two directions:

1. **Request**

* the lua coroutine creates a task
* associate the task with request message, which is C `malloc()` char array
* put the task into the thread-safe queue of the runtime and yield
* the runtime polls this queue

Why not call API provided by other languages?
* In Go, due to GMP model, it may block the nginx worker process
* It increases the burden for other languages to provide such API

2. **Response**

The runtime injects the response (also C `malloc()` char array)
into the `ngx_thread_pool_done` queue directly and notify the nginx epoll loop via eventfd,
the nginx would resumes the lua coroutine then.

By contrast, the thread pool way to get response is:
* low-efficient (extra thread context switchs)
* nonscaleable (consumption of linux threads, then the throughput is limited by the thread pool size)
* complicated (the runtime needs to assign the request with id and store them in hashmap like structure for access).

## Lua API

### `local runtime = ngx.load_nonblocking_ffi(lib, cfg, opts)`

Load and return the runtime

* `lib`
shared library name. It could be absolute file path or name only,
or even short name (e.g. for `libdemo.so`, the short name is `demo`).
When the `lib` is in name, it's searched according to `LD_LIBRARY_PATH` environment variable.

* `cfg` configuration, it could be string or nil.

* `opts` options table.

```lua
{
    -- the maximum queue size for pending requests to the runtime.
    max_queue = 65536,
    -- denotes whether the symbols loaded from library
    -- would be exported in global namespace, which is necessary for python3.
    is_global = false,
}
```

This API is idempotent. The loaded runtime is cached in internal table, where
the table key is `lib .. '&' .. cfg`.

This function calls the `lib_nonblocking_ffi_init` of the library per key.

It means the same library with different configuration would init a different new runtime,
which is especially useful for python3 and Java.

Example:

```lua
local opts = {is_global = true}
local demo = ngx.load_nonblocking_ffi("lib_nonblocking_ffi_python3.so",
    [[nffi_kafka,init,{"servers":"localhost:9092", "topic":"foobar", "group_id": "foobar"}]], opts)
```

### `local ok, res = runtime:foo(req)`

Send a request to the rutnime and returns the response.

* `req` the request string, could be in any serialization format, e.g. json, protobuf, as long as it matches the runtime implementation.

* `ok` return status, true or false

* `res` response string, could be in any serialization format, e.g. json, protobuf, as long as it matches the runtime implementation.

This method is nonblocking, which means the coroutine would yield waiting for the response and resume with the return values.

Note that the method name `foo` could be any name you like, it would be generated automatically by the `__index` meta function, and only used to denote the request semantics。

Example:

```lua
local ok, res
ok, res = demo:produce([[{"type":"produce", "msg":"hello"}]])
assert(ok)
ok, res = demo:produce([[{"type":"produce", "msg":"world"}]])
assert(ok)
ngx.sleep(2)
ok, res = demo:consume([[{"type":"consume"}]])
assert(ok)
ngx.say(res)

```

### `local ok, res = runtime:__unload()`

Unload the runtime, after that, no request could be sent to this runtime anymore.

## API provided by runtime

### `int lib_nonblocking_ffi_init(char* cfg, void *tq);`

This API is provided by the library to initiate its logic and start the poll thread/goroutine.

Example:

```go
//export lib_nonblocking_ffi_init
func lib_nonblocking_ffi_init(cfg *C.char, tq unsafe.Pointer) C.int {
    var etcdNodes []string
    data := C.GoString(cfg)
    err := json.Unmarshal([]byte(data), &etcdNodes)
    if err != nil {
        log.Println("invalid cfg:", err)
        return -1
    }

    cli, err := clientv3.New(clientv3.Config{
        Endpoints:   etcdNodes,
        DialTimeout: 5 * time.Second,
    })
    ...
    go func() {
        isWatchStart := false
        state := &State{cli: cli}
        for {
            task := C.ngx_http_lua_nonblocking_ffi_task_poll(tq)
            ...
        }
    }()
    return 0
}
```

## APIs used by runtime

### `void* ngx_http_lua_nonblocking_ffi_task_poll(void *p);`

Poll the task from the task queue assigned to the runtime.

When it returns `NULL`, it denotes the runtime was unloaded, the polling thread/goroutine should clean up
everything of this runtime and do not access the task queue anymore (because the task queue was deallocated in C)!

### `char* ngx_http_lua_nonblocking_ffi_get_req(void *tsk, int *len);`

Extract the request from the task. Note that the request could be NULL, so the runtime may not use this API.

### `void ngx_http_lua_nonblocking_ffi_respond(void *tsk, int rc, char* rsp, int rsp_len);`

Response the task.

All above APIs are thread-safe. So you could use them in anywhere in your thread/goroutine of your runtime.

* `rc` return status, `0` means successful, other values means failure.
* `rsp` response char array, may be NULL if the runtime does not need to response something.
* `rsp_len` the length of response, may be `0` if the `rsp` is NULL or `\0' terminated C string.

## Build

```bash
# OS: Ubuntu 20.04

apt install -y build-essential

cd /opt
git clone https://github.com/kingluo/lua-resty-nonblocking-ffi
wget https://openresty.org/download/openresty-1.21.4.1.tar.gz
tar zxf openresty-1.21.4.1.tar.gz

# patch and install source codes
cd /opt/openresty-1.21.4.1/bundle/ngx_lua-0.10.21/
patch -p1 < /opt/lua-resty-nonblocking-ffi/patches/config.patch
cd /opt/openresty-1.21.4.1/bundle/lua-resty-core-0.1.23/lib/resty/
patch -p1 < /opt/lua-resty-nonblocking-ffi/patches/core.lua.patch
cd /opt/openresty-1.21.4.1/bundle/nginx-1.21.4/src/core/
patch -p1 < /opt/lua-resty-nonblocking-ffi/patches/ngx_thread_pool.c.patch

cd /opt/lua-resty-nonblocking-ffi/
cp -a ngx_http_lua_nonblocking_ffi.c /opt/openresty-1.21.4.1/bundle/ngx_lua-0.10.21/src/
cp -a nonblocking_ffi.lua /opt/openresty-1.21.4.1/bundle/lua-resty-core-0.1.23/lib/resty/core/

# compile and install openresty
cd /opt/openresty-1.21.4.1
./configure --prefix=/opt/nffi --with-threads
make install

# the openresty is installed in /opt/nffi/
```
