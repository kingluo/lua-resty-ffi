package demo.echo;

import jnr.ffi.LibraryLoader;
import jnr.ffi.*;
import jnr.ffi.byref.*;
import jnr.ffi.annotations.In;
import jnr.ffi.annotations.Out;

class PollThread implements Runnable {
    public interface FFI {
        Pointer ngx_http_lua_nonblocking_ffi_task_poll(@In Pointer tq);
        Pointer ngx_http_lua_nonblocking_ffi_get_req(@In Pointer task, @Out IntByReference len);
        void ngx_http_lua_nonblocking_ffi_respond(@In Pointer tsk, @In int rc, @In Pointer rsp, @In int rsp_len);
    }

    public interface Libc {
        Pointer malloc(int size);
        Pointer memcpy(Pointer dest, Pointer src, int n);
    }

    public long tq;

    public PollThread(long tq) {
        this.tq = tq;
    }

    public void run() {
        Libc libc = LibraryLoader.create(Libc.class).load("c");
        FFI ffi = LibraryLoader.create(FFI.class).load(LibraryLoader.DEFAULT_LIBRARY);
        var tq = Pointer.wrap(jnr.ffi.Runtime.getSystemRuntime(), this.tq);
        IntByReference ap = new IntByReference();

        while(true) {
            var task = ffi.ngx_http_lua_nonblocking_ffi_task_poll(tq);
            if (task == null) {
                break;
            }
            var req = ffi.ngx_http_lua_nonblocking_ffi_get_req(task, ap);
            var len = ap.intValue();

            var rsp = libc.malloc(len);
            libc.memcpy(rsp, req, len);
            ffi.ngx_http_lua_nonblocking_ffi_respond(task, 0, rsp, len);
        }

        System.out.println("exit java echo runtime");
    }
}

public class App
{
    public static int init(String cfg, long tq) {
        var pollThread = new PollThread(tq);
        Thread thread = new Thread(pollThread);
        thread.setDaemon(true);
        thread.start();
        return 0;
    }
}
