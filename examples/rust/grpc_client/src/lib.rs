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
use tokio::sync::mpsc;
use tonic::transport::{Certificate, Channel, ClientTlsConfig, Identity, Uri};
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

const NEW_CONNECTION: i32 = 0;
const CLOSE_CONNECTION: i32 = 1;
const UNARY: i32 = 2;
const NEW_STREAM: i32 = 3;
const CLOSE_STREAM: i32 = 4;
const CLOSE_SEND: i32 = 5;
const STREAM_SEND: i32 = 6;
const STREAM_RECV: i32 = 7;

#[derive(Deserialize, Debug)]
pub struct GrpcCommand {
    cmd: i32,
    key: String,
    host: Option<String>,
    ca: Option<String>,
    cert: Option<String>,
    priv_key: Option<String>,
    path: Option<String>,
    payload: Option<Base64>,
}

#[derive(Clone)]
struct TaskQueueHandle(*const c_void);
unsafe impl Send for TaskQueueHandle {}
unsafe impl Sync for TaskQueueHandle {}

#[derive(Clone, Debug)]
struct TaskHandle(*const c_void);
unsafe impl Send for TaskHandle {}
unsafe impl Sync for TaskHandle {}

extern "C" {
    fn ngx_http_lua_ffi_task_poll(p: *const c_void) -> *const c_void;
    fn ngx_http_lua_ffi_get_req(tsk: *const c_void, len: *mut c_int) -> *mut c_char;
    fn ngx_http_lua_ffi_respond(tsk: *const c_void, rc: c_int, rsp: *const c_char, rsp_len: c_int);
}

struct EncodedBytes(*mut c_char, usize, bool);
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
        if item.2 {
            unsafe {
                libc::free(item.0 as *mut c_void);
            }
        }
        Ok(())
    }
}

impl Decoder for MyCodec {
    type Item = EncodedBytes;
    type Error = Status;

    fn decode(&mut self, buf: &mut DecodeBuf<'_>) -> Result<Option<Self::Item>, Self::Error> {
        let chunk = buf.chunk();
        let len = chunk.len();
        if len > 0 {
            let cbuf = unsafe { libc::malloc(len) };
            unsafe { libc::memcpy(cbuf, chunk.as_ptr() as *const c_void, len) };
            buf.advance(len);
            Ok(Some(EncodedBytes(cbuf as *mut c_char, len, false)))
        } else {
            Ok(Some(EncodedBytes(
                std::ptr::null_mut() as *mut c_char,
                0,
                false,
            )))
        }
    }
}

#[no_mangle]
pub extern "C" fn libffi_init(_cfg: *mut c_char, tq: *const c_void) -> c_int {
    let tq = TaskQueueHandle(tq);

    let rt = tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .thread_name("grpc-client-thread")
        .build()
        .unwrap();

    thread::spawn(move || {
        rt.block_on(async move {
            let obj_cnt = Arc::new(AtomicU64::new(1));
            let clients = Arc::new(Mutex::new(HashMap::new()));
            let streams = Arc::new(Mutex::new(HashMap::new()));
            let tq = tq.clone();

            loop {
                let task = unsafe { TaskHandle(ngx_http_lua_ffi_task_poll(tq.0)) };
                if task.0.is_null() {
                    break;
                }

                let req_len = Box::<c_int>::new(0);
                let ptr = Box::into_raw(req_len);
                let req = unsafe { ngx_http_lua_ffi_get_req(task.0, ptr) };

                let sli = unsafe { std::slice::from_raw_parts(req as *const u8, *ptr as usize) };
                let mut cmd: GrpcCommand = serde_json::from_slice(sli).unwrap();
                //println!("cmd: {:?}", cmd);

                match cmd.cmd {
                    NEW_CONNECTION => {
                        let obj_cnt = obj_cnt.clone();
                        let clients = clients.clone();
                        tokio::spawn(async move {
                            let task = task.clone();
                            if !cmd.key.starts_with("http://") && !cmd.key.starts_with("https://") {
                                cmd.key = format!("http://{}", cmd.key);
                            }

                            let cli = if cmd.ca.is_some() {
                                let ca = Certificate::from_pem(cmd.ca.unwrap());
                                let mut tls = ClientTlsConfig::new().ca_certificate(ca);

                                if cmd.host.is_some() {
                                    tls = tls.domain_name(cmd.host.unwrap());
                                }

                                if cmd.cert.is_some() {
                                    let identity = Identity::from_pem(
                                        cmd.cert.unwrap(),
                                        cmd.priv_key.unwrap(),
                                    );
                                    tls = tls.identity(identity);
                                }

                                let channel = Channel::builder(Uri::from_str(&cmd.key).unwrap())
                                    .tls_config(tls)
                                    .unwrap()
                                    .connect()
                                    .await
                                    .unwrap();

                                tonic::client::Grpc::new(channel)
                            } else {
                                let ep = tonic::transport::Endpoint::new(cmd.key)
                                    .unwrap()
                                    .connect()
                                    .await
                                    .unwrap();
                                tonic::client::Grpc::new(ep)
                            };

                            let id = format!("cli-{}", obj_cnt.fetch_add(1, Ordering::SeqCst));
                            unsafe {
                                let res = libc::malloc(id.len());
                                libc::memcpy(res, id.as_ptr() as *const c_void, id.len());
                                let len = id.len();
                                clients.lock().unwrap().insert(id, cli);
                                ngx_http_lua_ffi_respond(task.0, 0, res as *mut c_char, len as i32);
                            }
                        });
                    }
                    CLOSE_CONNECTION => {
                        clients.lock().unwrap().remove(&cmd.key);
                        unsafe {
                            ngx_http_lua_ffi_respond(task.0, 0, std::ptr::null_mut(), 0);
                        }
                    }
                    UNARY => {
                        let mut cli = clients.lock().unwrap().get(&cmd.key).unwrap().clone();
                        tokio::spawn(async move {
                            let task = task.clone();
                            let mut vec = cmd.payload.unwrap().0;
                            let buf = EncodedBytes(
                                vec.as_mut_slice().as_mut_ptr() as *mut c_char,
                                vec.len(),
                                false,
                            );
                            cli.ready().await.unwrap();
                            let path = PathAndQuery::from_str(cmd.path.as_ref().unwrap()).unwrap();
                            let res = cli.unary(tonic::Request::new(buf), path, MyCodec).await;
                            unsafe {
                                if let Ok(mut res) = res {
                                    ngx_http_lua_ffi_respond(
                                        task.0,
                                        0,
                                        res.get_mut().0,
                                        res.get_ref().1 as i32,
                                    );
                                } else {
                                    ngx_http_lua_ffi_respond(task.0, 1, std::ptr::null_mut(), 0);
                                }
                            }
                        });
                    }
                    NEW_STREAM => {
                        let obj_cnt = obj_cnt.clone();
                        let clients = clients.clone();
                        let streams = streams.clone();
                        tokio::spawn(async move {
                            let task = task.clone();
                            let mut cli = clients.lock().unwrap().get(&cmd.key).unwrap().clone();
                            let (send_tx, send_rx) = mpsc::unbounded_channel::<EncodedBytes>();
                            let send_stream =
                                tokio_stream::wrappers::UnboundedReceiverStream::new(send_rx);
                            let (recv_tx, mut recv_rx) = mpsc::unbounded_channel::<TaskHandle>();
                            let path = PathAndQuery::from_str(cmd.path.as_ref().unwrap()).unwrap();
                            cli.ready().await.unwrap();
                            unsafe {
                                let id =
                                    format!("stream-{}", obj_cnt.fetch_add(1, Ordering::SeqCst));
                                let res = libc::malloc(id.len());
                                libc::memcpy(res, id.as_ptr() as *const c_void, id.len());
                                let len = id.len();
                                streams.lock().unwrap().insert(id, (Some(send_tx), recv_tx));
                                ngx_http_lua_ffi_respond(task.0, 0, res as *mut c_char, len as i32);
                            }
                            let mut stream = cli
                                .streaming(tonic::Request::new(send_stream), path, MyCodec)
                                .await
                                .unwrap()
                                .into_inner();

                            while let Some(task) = recv_rx.recv().await {
                                if let Some(res) = stream.message().await.unwrap() {
                                    unsafe {
                                        ngx_http_lua_ffi_respond(task.0, 0, res.0, res.1 as i32);
                                    }
                                } else {
                                    unsafe {
                                        ngx_http_lua_ffi_respond(
                                            task.0,
                                            0,
                                            std::ptr::null_mut(),
                                            0,
                                        );
                                    }
                                }
                            }
                        });
                    }
                    STREAM_SEND => {
                        let (send_tx, _) = streams.lock().unwrap().get(&cmd.key).unwrap().clone();
                        let send_tx = send_tx.unwrap();
                        let mut vec = cmd.payload.unwrap().0;
                        let cbuf = unsafe { libc::malloc(vec.len()) };
                        unsafe {
                            libc::memcpy(
                                cbuf,
                                vec.as_mut_slice().as_mut_ptr() as *const c_void,
                                vec.len(),
                            )
                        };
                        let buf = EncodedBytes(cbuf as *mut c_char, vec.len(), true);
                        unsafe {
                            ngx_http_lua_ffi_respond(
                                task.0,
                                if send_tx.send(buf).is_ok() { 0 } else { 1 },
                                std::ptr::null_mut(),
                                0,
                            );
                        }
                    }
                    STREAM_RECV => {
                        let (_, recv_tx) = streams.lock().unwrap().get(&cmd.key).unwrap().clone();
                        let task = task.clone();
                        recv_tx.send(task).unwrap();
                    }
                    CLOSE_STREAM => {
                        streams.lock().unwrap().remove(&cmd.key);
                        unsafe {
                            ngx_http_lua_ffi_respond(task.0, 0, std::ptr::null_mut(), 0);
                        }
                    }
                    CLOSE_SEND => {
                        let (_, recv_tx) = streams.lock().unwrap().remove(&cmd.key).unwrap();
                        streams.lock().unwrap().insert(cmd.key, (None, recv_tx));
                        unsafe {
                            ngx_http_lua_ffi_respond(task.0, 0, std::ptr::null_mut(), 0);
                        }
                    }
                    _ => todo!(),
                }
            }
        });

        println!("shutdown tokio runtime");
        rt.shutdown_background();
    });

    return 0;
}
