from cffi import FFI
ffi = FFI()
ffi.cdef("""
void* malloc(size_t);
void *memcpy(void *dest, const void *src, size_t n);
void* ngx_http_lua_ffi_task_poll(void *p);
char* ngx_http_lua_ffi_get_req(void *tsk, int *len);
void ngx_http_lua_ffi_respond(void *tsk, int rc, char* rsp, int rsp_len);
""")
C = ffi.dlopen(None)

import threading
import json
import queue
import time
from kafka import KafkaProducer
from kafka import KafkaConsumer
from kafka import KafkaAdminClient
from kafka.admin import NewTopic
from kafka.errors import TopicAlreadyExistsError

class State:
    def __init__(self, cfg):
        self.topic = cfg["topic"]
        servers = cfg["servers"]

        self.taskq = queue.Queue()
        self.consume_taskq = queue.Queue()

        self.producer = KafkaProducer(bootstrap_servers=servers)
        self.consumer = KafkaConsumer(bootstrap_servers = servers, group_id = cfg["group_id"], enable_auto_commit = True)
        self.consumer.subscribe([self.topic])

        try:
            admin = KafkaAdminClient(bootstrap_servers=servers)
            topic_list = []
            topic_list.append(NewTopic(name=self.topic, num_partitions=1, replication_factor=1))
            print(admin.create_topics(new_topics=topic_list, validate_only=False))
            msgs = self.consumer.poll()
            assert not msgs
        except TopicAlreadyExistsError as err:
            pass

        t = threading.Thread(target=self.consumer_loop)
        t.daemon = True
        t.start()
        self.consumer_thread = t

        t = threading.Thread(target=self.producer_loop)
        t.daemon = True
        t.start()
        self.task_loop_thread = t

    def respond(self, task, data):
        res = C.malloc(len(data))
        C.memcpy(res, data.encode(), len(data))
        C.ngx_http_lua_ffi_respond(task, 0, res, len(data))

    def consumer_loop(self):
        while True:
            task, _ = self.consume_taskq.get()
            msg = next(self.consumer)
            ev = {
                "topic": msg.topic,
                "partition": msg.partition,
                "timestamp": msg.timestamp,
                "key": msg.key,
                "value": msg.value.decode(),
            }
            data = json.dumps(ev)
            self.respond(task, data)
            self.consume_taskq.task_done()

    def producer_loop(self):
        while True:
            task, req = self.taskq.get()
            msg = req["msg"]
            future = self.producer.send(self.topic, msg.encode())
            result = future.get(timeout=60)
            print(result, msg)
            C.ngx_http_lua_ffi_respond(task, 0, ffi.NULL, 0)
            self.taskq.task_done()

    def poll(self, tq):
        while True:
            task = C.ngx_http_lua_ffi_task_poll(ffi.cast("void*", tq))
            r = C.ngx_http_lua_ffi_get_req(task, ffi.NULL)
            req = json.loads(ffi.string(r))
            typ = req["type"]
            if typ == "produce":
                self.taskq.put((task, req))
            elif typ == "consume":
                self.consume_taskq.put((task, req))
            else:
                C.ngx_http_lua_ffi_respond(task, -1, ffi.NULL, 0)

def init(cfg, tq):
    data = ffi.string(ffi.cast("char*", cfg))
    cfg = json.loads(data)
    st = State(cfg)
    t = threading.Thread(target=st.poll, args=(tq,))
    t.daemon = True
    t.start()
    return 0
