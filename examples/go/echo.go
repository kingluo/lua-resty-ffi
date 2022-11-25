package main

/*
#cgo LDFLAGS: -shared
#include <string.h>
void* ngx_http_lua_ffi_task_poll(void *p);
char* ngx_http_lua_ffi_get_req(void *tsk, int *len);
void ngx_http_lua_ffi_respond(void *tsk, int rc, char* rsp, int rsp_len);
*/
import "C"
import (
	"log"
	"unsafe"
)

//export libffi_init
func libffi_init(cfg_cstr *C.char, tq unsafe.Pointer) C.int {
	log.Println("start go echo runtime")
	go func() {
		for {
			task := C.ngx_http_lua_ffi_task_poll(tq)
			if task == nil {
				break
			}
			var rlen C.int
			r := C.ngx_http_lua_ffi_get_req(task, &rlen)
			res := C.malloc(C.ulong(rlen))
			C.memcpy(res, unsafe.Pointer(r), C.ulong(rlen))
			C.ngx_http_lua_ffi_respond(task, 0, (*C.char)(res), rlen)
		}
		log.Println("exit go echo runtime")
	}()
	return 0
}

func main() {}
