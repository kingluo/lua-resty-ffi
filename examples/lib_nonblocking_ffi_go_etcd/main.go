package main

/*
#cgo LDFLAGS: -shared
#include <string.h>
void* ngx_http_lua_nonblocking_ffi_task_poll(void *p);
char* ngx_http_lua_nonblocking_ffi_get_req(void *tsk, int *len);
void ngx_http_lua_nonblocking_ffi_respond(void *tsk, int rc, char* rsp, int rsp_len);
*/
import "C"
import (
	"bytes"
	"context"
	"encoding/json"
	"log"
	"sync"
	"time"
	"unsafe"

	clientv3 "go.etcd.io/etcd/client/v3"
)

type Request struct {
	Type    string          `json:"type"`
	Payload json.RawMessage `json:"payload"`
}

type Put struct {
	task  unsafe.Pointer `json:"-"`
	Key   string         `json:"key"`
	Value string         `json:"value"`
}

type Watch struct {
	task   unsafe.Pointer `json:"-"`
	events []*WatchEvent  `json:"-"`
	Prefix string         `json:"prefix"`
	Key    string         `json:"key"`
}

type WatchEvent struct {
	Key      string `json:"key"`
	Value    string `json:"value"`
	Revision int64  `json:"revision"`
}

type State struct {
	watchesMutex sync.Mutex
	watches      []*Watch
	cli          *clientv3.Client
}

func (state *State) watch(prefix string) {
	rch := state.cli.Watch(context.Background(), prefix, clientv3.WithPrefix())
	for wresp := range rch {
		var pendingWatches []*Watch
		var doneWatches []*Watch

		state.watchesMutex.Lock()
		for _, ev := range wresp.Events {
			log.Printf("%s %q : %q\n", ev.Type, ev.Kv.Key, ev.Kv.Value)
			watchEv := &WatchEvent{Key: string(ev.Kv.Key), Value: string(ev.Kv.Value), Revision: wresp.Header.Revision}
			for _, w := range state.watches {
				if bytes.HasPrefix(ev.Kv.Key, []byte(w.Key)) {
					w.events = append(w.events, watchEv)
				}
			}
		}
		for _, w := range state.watches {
			if len(w.events) == 0 {
				pendingWatches = append(pendingWatches, w)
			} else {
				doneWatches = append(doneWatches, w)
			}
		}
		state.watches = pendingWatches
		state.watchesMutex.Unlock()

		for _, w := range doneWatches {
			events, err := json.Marshal(w.events)
			if err != nil {
				log.Fatalln("put failed, err:", err)
			}
			C.ngx_http_lua_nonblocking_ffi_respond(w.task, 0, (*C.char)(C.CBytes(events)), C.int(len(events)))
		}
	}
}

func (state *State) put(p *Put) {
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	_, err := state.cli.Put(ctx, p.Key, p.Value)
	cancel()
	var rc C.int
	if err != nil {
		log.Println("put failed, err:", err)
		rc = -1
	}
	C.ngx_http_lua_nonblocking_ffi_respond(p.task, rc, nil, 0)
}

//export lib_nonblocking_ffi_init
func lib_nonblocking_ffi_init(cfg *C.char, cfg_len C.int, tq unsafe.Pointer) C.int {
	var etcdNodes []string
	data := C.GoBytes(unsafe.Pointer(cfg), cfg_len)
	err := json.Unmarshal(data, &etcdNodes)
	if err != nil {
		log.Println("invalid cfg:", err)
		return -1
	}

	cli, err := clientv3.New(clientv3.Config{
		Endpoints:   etcdNodes,
		DialTimeout: 5 * time.Second,
	})
	if err != nil {
		log.Println("connect etcd failed:", err)
		return -1
	}

	go func() {
		isWatchStart := false
		state := &State{cli: cli}
		for {
			task := C.ngx_http_lua_nonblocking_ffi_task_poll(tq)

			var rlen C.int
			r := C.ngx_http_lua_nonblocking_ffi_get_req(task, &rlen)
			data := C.GoBytes(unsafe.Pointer(r), rlen)
			var req Request
			err := json.Unmarshal(data, &req)
			if err != nil {
				log.Fatalln("error:", err)
			}

			switch req.Type {
			case "put":
				payload := new(Put)
				err := json.Unmarshal(req.Payload, payload)
				if err != nil {
					log.Fatalln("error:", err)
				}
				payload.task = task
				log.Println(req.Type, payload)
				go state.put(payload)
			case "watch":
				payload := new(Watch)
				err := json.Unmarshal(req.Payload, payload)
				if err != nil {
					log.Fatalln("error:", err)
				}
				payload.task = task
				log.Println(req.Type, payload)

				state.watchesMutex.Lock()
				state.watches = append(state.watches, payload)
				state.watchesMutex.Unlock()

				if !isWatchStart {
					go state.watch(payload.Prefix)
					isWatchStart = true
				}
			}
		}
	}()

	return 0
}

func main() {}
