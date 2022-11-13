from nffi import *

import threading
import json
import queue
from kafka import KafkaProducer
from kafka import KafkaConsumer

class State:
    def __init__(self, cfg):
        self.taskq = queue.Queue()

        self.topic = cfg["topic"]
        servers = cfg["servers"]
        self.producer = KafkaProducer(bootstrap_servers=servers)
        self.consumer = KafkaConsumer(bootstrap_servers=servers, group_id=cfg["group_id"], enable_auto_commit=True)
        self.consumer.subscribe([self.topic])

        self.msgs = []
        self.consume_tasks = []
        self.mutex = threading.Lock()

        t = threading.Thread(target=self.consume)
        t.daemon = True
        t.start()
        self.consumer_thread = t

        t = threading.Thread(target=self.task_loop)
        t.daemon = True
        t.start()
        self.task_loop_thread = t

    def produce(self, msg):
        self.producer.send(self.topic, msg.encode())

    def respond(self, task, data):
        res = C.malloc(len(data))
        C.memcpy(res, data.encode(), len(data))
        C.ngx_http_lua_nonblocking_ffi_respond(task, 0, res, len(data))

    def consume(self):
        for msg in self.consumer:
            ev = {
                "topic": msg.topic,
                "partition": msg.partition,
                "timestamp": msg.timestamp,
                "key": msg.key,
                "value": msg.value.decode(),
            }
            self.mutex.acquire()
            try:
                if not self.consume_tasks:
                    self.msgs.append(ev)
                else:
                    data = json.dumps(ev)
                    for task in self.consume_tasks:
                        self.respond(task, data)
                    self.consume_tasks.clear()
            finally:
                self.mutex.release()

    def task_loop(self):
        while True:
            task, req = self.taskq.get()
            req = json.loads(ffi.string(req))
            typ = req["type"]
            if typ == "produce":
                self.produce(req["msg"])
                C.ngx_http_lua_nonblocking_ffi_respond(task, 0, ffi.NULL, 0)
            elif typ == "consume":
                self.mutex.acquire()
                try:
                    if self.msgs:
                        data = json.dumps(self.msgs)
                        self.msgs.clear()
                        self.respond(task, data)
                    else:
                        self.consume_tasks.append(task)
                finally:
                    self.mutex.release()
            else:
                C.ngx_http_lua_nonblocking_ffi_respond(task, -1, ffi.NULL, 0)
            self.taskq.task_done()

    def poll(self, tq):
        while True:
            task = C.ngx_http_lua_nonblocking_ffi_task_poll(ffi.cast("void*", tq))
            r = C.ngx_http_lua_nonblocking_ffi_get_req(task, ffi.NULL)
            self.taskq.put((task, r))

def init(cfg, tq):
    data = ffi.string(ffi.cast("char*", cfg))
    cfg = json.loads(data)
    st = State(cfg)
    t = threading.Thread(target=st.poll, args=(tq,))
    t.daemon = True
    t.start()
    return 0
