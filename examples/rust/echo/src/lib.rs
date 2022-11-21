use serde_json::Value;
use std::ffi::{c_char, c_int, c_void, CStr};
use std::thread;

#[derive(Clone)]
struct TaskQueueHandle(*const c_void);
unsafe impl Send for TaskQueueHandle {}
unsafe impl Sync for TaskQueueHandle {}

extern "C" {
    fn ngx_http_lua_ffi_task_poll(p: *const c_void) -> *const c_void;
    fn ngx_http_lua_ffi_get_req(tsk: *const c_void, len: *mut c_int) -> *mut c_char;
    fn ngx_http_lua_ffi_respond(tsk: *const c_void, rc: c_int, rsp: *const c_char, rsp_len: c_int);
}

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
                let task = ngx_http_lua_ffi_task_poll(tq.0);
                if task.is_null() {
                    break;
                }
                let req = ngx_http_lua_ffi_get_req(task, nullptr);
                let res = libc::strdup(req);
                ngx_http_lua_ffi_respond(task, 0, res, 0);
            }
        }
        println!("exit echo runtime");
    });

    return 0;
}
