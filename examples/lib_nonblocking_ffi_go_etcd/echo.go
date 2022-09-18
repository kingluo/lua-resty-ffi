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
	"io"
	"log"
	"net"
	"os"
	"time"
	"unsafe"
)

const SockAddr = "/tmp/echo.sock"

func echoServer(c net.Conn) {
	log.Println("* start echo io")
	n, _ := io.Copy(c, c)
	log.Println("* end echo io, copy: ", n)
	c.Close()
}

//export lib_nonblocking_ffi_init
func lib_nonblocking_ffi_init(cfg *C.char, cfg_len C.int, tq unsafe.Pointer) C.int {
	log.Println("cfg_len: ", cfg_len)
	if cfg_len > 0 {
		log.Println("start unix server")
		go func() {
			if err := os.RemoveAll(SockAddr); err != nil {
				log.Fatal(err)
			}

			l, err := net.Listen("unix", SockAddr)
			if err != nil {
				log.Fatal("listen error:", err)
			}
			defer l.Close()
			for {
				log.Println("accept...")
				conn, err := l.Accept()
				if err != nil {
					log.Fatal("accept error:", err)
				}

				go echoServer(conn)
			}
		}()
		time.Sleep(5 * time.Second)
		log.Println("wait end")
	} else {
		log.Println("start nffi-echo")
		go func() {
			for {
				task := C.ngx_http_lua_nonblocking_ffi_task_poll(tq)
				var rlen C.int
				r := C.ngx_http_lua_nonblocking_ffi_get_req(task, &rlen)
				res := C.malloc(C.ulong(rlen))
				C.memcpy(res, unsafe.Pointer(r), C.ulong(rlen))
				C.ngx_http_lua_nonblocking_ffi_respond(task, 0, (*C.char)(res), rlen)
			}
		}()
	}
	return 0
}

func main() {}
