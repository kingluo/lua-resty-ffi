use bytes::{Buf, BufMut};
use http::uri::PathAndQuery;
use serde::de::Error;
use serde::{Deserialize, Deserializer};
use std::collections::HashMap;
use std::ffi::{c_char, c_int, c_void};
use std::slice;
use std::str::FromStr;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use tonic::{
    codec::{Codec, DecodeBuf, Decoder, EncodeBuf, Encoder},
    Status,
};

#[derive(Debug)]
pub struct Base64(Vec<u8>);

impl<'de> Deserialize<'de> for Base64 {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        struct Vis;
        impl serde::de::Visitor<'_> for Vis {
            type Value = Base64;

            fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
                formatter.write_str("a base64 string")
            }

            fn visit_str<E: Error>(self, v: &str) -> Result<Self::Value, E> {
                base64::decode(v).map(Base64).map_err(Error::custom)
            }
        }
        deserializer.deserialize_str(Vis)
    }
}

#[derive(Deserialize, Debug)]
pub struct GrpcCommand {
    cmd: i32,
    key: String,
    path: Option<String>,
    payload: Option<Base64>,
}

#[derive(Clone)]
struct TaskQueueHandle(*const c_void);
unsafe impl Send for TaskQueueHandle {}
unsafe impl Sync for TaskQueueHandle {}

extern "C" {
    fn ngx_http_lua_nonblocking_ffi_task_poll(p: *const c_void) -> *const c_void;
    fn ngx_http_lua_nonblocking_ffi_get_req(tsk: *const c_void, len: *mut c_int) -> *mut c_char;
    fn ngx_http_lua_nonblocking_ffi_respond(
        tsk: *const c_void,
        rc: c_int,
        rsp: *const c_char,
        rsp_len: c_int,
    );
}

struct EncodedBytes(*mut c_char, usize);
unsafe impl Send for EncodedBytes {}
unsafe impl Sync for EncodedBytes {}

struct MyCodec;

impl Codec for MyCodec {
    type Encode = EncodedBytes;
    type Decode = EncodedBytes;

    type Encoder = Self;
    type Decoder = Self;

    fn encoder(&mut self) -> Self::Encoder {
        MyCodec
    }

    fn decoder(&mut self) -> Self::Decoder {
        MyCodec
    }
}

impl Encoder for MyCodec {
    type Item = EncodedBytes;
    type Error = Status;

    fn encode(&mut self, item: Self::Item, dst: &mut EncodeBuf<'_>) -> Result<(), Self::Error> {
        let slice = unsafe { slice::from_raw_parts(item.0 as *const u8, item.1) };
        dst.put_slice(slice);
        Ok(())
    }
}

impl Decoder for MyCodec {
    type Item = EncodedBytes;
    type Error = Status;

    fn decode(&mut self, buf: &mut DecodeBuf<'_>) -> Result<Option<Self::Item>, Self::Error> {
        let chunk = buf.chunk();
        let len = chunk.len();
        let cbuf = unsafe { libc::malloc(len) };
        unsafe { libc::memcpy(cbuf, chunk.as_ptr() as *const c_void, len) };
        buf.advance(len);
        Ok(Some(EncodedBytes(cbuf as *mut c_char, len)))
    }
}

#[no_mangle]
pub extern "C" fn lib_nonblocking_ffi_init(_cfg: *mut c_char, tq: *const c_void) -> c_int {
    let tq = TaskQueueHandle(tq);

    let rt = tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .worker_threads(4)
        .thread_name("grpc-client-thread")
        .thread_stack_size(3 * 1024 * 1024)
        .build()
        .unwrap();

    thread::spawn(move || {
        rt.block_on(async move {
            let client_cnt = Arc::new(AtomicU64::new(1));
            let clients = Arc::new(Mutex::new(HashMap::new()));
            let tq = tq.clone();

            loop {
                let task = unsafe { TaskQueueHandle(ngx_http_lua_nonblocking_ffi_task_poll(tq.0)) };
                if task.0.is_null() {
                    break;
                }

                let req_len = Box::<c_int>::new(0);
                let ptr = Box::into_raw(req_len);
                let req = unsafe { ngx_http_lua_nonblocking_ffi_get_req(task.0, ptr) };

                let sli = unsafe { std::slice::from_raw_parts(req as *const u8, *ptr as usize) };
                let cmd: GrpcCommand = serde_json::from_slice(sli).unwrap();
                //println!("cmd: {:?}", cmd);

                match cmd.cmd {
                    0 => {
                        let client_cnt = client_cnt.clone();
                        let clients = clients.clone();
                        tokio::spawn(async move {
                            let task = task.clone();
                            let cli = tonic::transport::Endpoint::new(cmd.key)
                                .unwrap()
                                .connect()
                                .await
                                .unwrap();
                            let cli = tonic::client::Grpc::new(cli);
                            let id = format!("cli-{}", client_cnt.fetch_add(1, Ordering::SeqCst));
                            unsafe {
                                let res = libc::malloc(id.len());
                                libc::memcpy(res, id.as_ptr() as *const c_void, id.len());
                                let len = id.len();
                                clients.lock().unwrap().insert(id, cli);
                                ngx_http_lua_nonblocking_ffi_respond(
                                    task.0,
                                    0,
                                    res as *mut c_char,
                                    len as i32,
                                );
                            }
                        });
                    }
                    1 => {
                        clients.lock().unwrap().remove(&cmd.key);
                        unsafe {
                            ngx_http_lua_nonblocking_ffi_respond(
                                task.0,
                                0,
                                std::ptr::null_mut(),
                                0,
                            );
                        }
                    }
                    2 => {
                        let mut cli = clients.lock().unwrap().get(&cmd.key).unwrap().clone();
                        tokio::spawn(async move {
                            let task = task.clone();
                            let mut vec = cmd.payload.unwrap().0;
                            let buf = EncodedBytes(
                                vec.as_mut_slice().as_mut_ptr() as *mut c_char,
                                vec.len(),
                            );
                            cli.ready().await.unwrap();
                            let pp = cmd.path.unwrap();
                            let path = PathAndQuery::from_str(&pp).unwrap();
                            let res = cli.unary(tonic::Request::new(buf), path, MyCodec).await;
                            unsafe {
                                if let Ok(mut res) = res {
                                    ngx_http_lua_nonblocking_ffi_respond(
                                        task.0,
                                        0,
                                        res.get_mut().0,
                                        res.get_ref().1 as i32,
                                    );
                                } else {
                                    ngx_http_lua_nonblocking_ffi_respond(
                                        task.0,
                                        1,
                                        std::ptr::null_mut(),
                                        0,
                                    );
                                }
                            }
                        });
                    }
                    _ => todo!(),
                }
            }
        });
    });

    return 0;
}
