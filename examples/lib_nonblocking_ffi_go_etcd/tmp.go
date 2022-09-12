package main

/*
#cgo LDFLAGS: -shared
#include <string.h>
#include <ngx_http_lua_nonblocking_ffi.h>
*/
import "C"
import (
	"log"
	"unsafe"
)

//export lib_nonblocking_ffi_init
func lib_nonblocking_ffi_init(cfg *C.char, cfg_len C.int, tq unsafe.Pointer) C.int {
	go func() {
		for {
			task := C.ngx_http_lua_nonblocking_ffi_task_poll(tq)
			log.Println("tmp get task")
			C.ngx_http_lua_nonblocking_ffi_respond(task, 0, nil, 0)
		}
	}()
	return 0
}

func main() {}
