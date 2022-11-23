# Rust ffi library

* `echo`

Echo the request.

* `grpc_client`

Generic grpc client based on [`tonic`](https://github.com/hyperium/tonic) low-level APIs.

## Install rust if not yet

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source "$HOME/.cargo/env"
```

## Build and test

```bash
# in terminal-1

# Install protoc-3 if not yet
# https://grpc.io/docs/protoc-installation/
PB_REL="https://github.com/protocolbuffers/protobuf/releases"
curl -LO $PB_REL/download/v3.15.8/protoc-3.15.8-linux-x86_64.zip
unzip protoc-3.15.8-linux-x86_64.zip -d /usr/local

# install lua-protobuf
# for centos7
source scl_source enable devtoolset-9
# for centos8
source /opt/rh/gcc-toolset-9/enable

luarocks install lua-protobuf

# build examples libraries
make

# run nginx
make run
# or specify nginx executable file path
# make run NGINX_BIN=/path/to/nginx

# in terminal-2

# run tonic grpc test server
cd /opt
git clone https://github.com/hyperium/tonic
cd tonic
cargo run --release --bin routeguide-server

# in terminal-3
make test

#
# test TLS
#

# in terminal-2
cargo run --release --bin tls-client-auth-server

# in terminal-3
curl localhost:20000/tls
```

## Library Skelton

```rust
#[no_mangle]
pub extern "C" fn libffi_init(cfg: *mut c_char, tq: *const c_void) -> c_int {
    let cfg = unsafe { CStr::from_ptr(cfg).to_string_lossy() };
    let cfg = serde_json::from_str::<Value>(&cfg);
    if !cfg.is_ok() {
        return 1;
    }
    let cfg: Value = cfg.unwrap();

    let tq = TaskQueueHandle(tq);
    thread::spawn(move || {
        println!("cfg: {:?}", cfg);
        let tq = tq.clone();
        let nullptr = std::ptr::null_mut();
        loop {
            unsafe {
                // poll a task
                let task = ngx_http_lua_ffi_task_poll(tq.0);
                // if the task queue is done, then quit
                if task.is_null() {
                    break;
                }
                // get the request from the task
                let req = ngx_http_lua_ffi_get_req(task, nullptr);
                // copy the request as response, allocated by C strdup()
                // note that both request and response would be freed by nginx
                let res = libc::strdup(req);
                ngx_http_lua_ffi_respond(task, 0, res, 0);
            }
        }
        println!("exit echo runtime");
    });

    return 0;
}
```
