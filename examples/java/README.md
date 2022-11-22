# JAVA ffi library

It uses [`jnr-ffi`](https://github.com/jnr/jnr-ffi) to call lua-resty-ffi C APIs.

* `echo-demo`

Echo the request.

* `http2-demo`

Simple http2 client. Requires openjdk >= 11.

## Install maven if not yet

```bash
wget --no-check-certificate https://dlcdn.apache.org/maven/maven-3/3.8.6/binaries/apache-maven-3.8.6-bin.tar.gz
rm -rf /usr/local/apache-maven-3.8.6 && tar -C /usr/local -xzf apache-maven-3.8.6-bin.tar.gz
export PATH=/usr/local/apache-maven-3.8.6/bin:$PATH
```

## Build and test

```bash
# in one terminal
# compile loader library
make

# run nginx
make run
# or specify nginx executable file path
# make run NGINX_BIN=/path/to/nginx

# in another terminal
# curl the tests
make test
```

## Library Skelton

```java
class PollThread implements Runnable {
    ...
    public void run() {
        Libc libc = LibraryLoader.create(Libc.class).load("c");
        FFI ffi = LibraryLoader.create(FFI.class).load(LibraryLoader.DEFAULT_LIBRARY);
        var tq = Pointer.wrap(jnr.ffi.Runtime.getSystemRuntime(), this.tq);
        IntByReference ap = new IntByReference();

        while(true) {
            // poll a task
            var task = ffi.ngx_http_lua_ffi_task_poll(tq);
            // if the task queue is done, then quit
            if (task == null) {
                break;
            }
            
            // get the request from the task
            var req = ffi.ngx_http_lua_ffi_get_req(task, ap);
            var len = ap.intValue();

            // copy the request as response, allocated by C malloc()
            // note that both request and response would be freed by nginx
            var rsp = libc.malloc(len);
            libc.memcpy(rsp, req, len);
            ffi.ngx_http_lua_ffi_respond(task, 0, rsp, len);
        }

        System.out.println("exit java echo runtime");
    }
}

public class App
{
    // implement an entry function
    public static int init(String cfg, long tq) {
        // launch a thread to poll tasks
        var pollThread = new PollThread(tq);
        Thread thread = new Thread(pollThread);
        thread.setDaemon(true);
        thread.start();
        return 0;
    }
}
```

Specify the entry class and function in lua and use it:

```lua
local demo = ngx.load_ffi("ffi_java", "demo.echo.App,init,")
local ok, res = demo:echo("hello")
assert(ok)
assert(res == "hello")
```
