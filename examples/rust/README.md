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
git clone https://github.com/hyperium/tonic
cd tonic
cargo run --release --bin routeguide-server

# in terminal-2

# build examples libraries
make

# run nginx
make run
# or specify nginx executable file path
# make run NGINX=/path/to/nginx

# in terminal-3
make test
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

    let handle = TaskQueueHandle(tq);
    let handle = Arc::new(Mutex::new(handle));
    thread::spawn(move || {
        println!("cfg: {:?}", cfg);
        let tq = handle.lock().unwrap().0;
        let nullptr = std::ptr::null_mut();
        loop {
            unsafe {
                // poll a task
                let task = ngx_http_lua_ffi_task_poll(tq);
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
