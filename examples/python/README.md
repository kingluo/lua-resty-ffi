# Python3 ffi library

It uses [`cffi`](https://cffi.readthedocs.io/) to call lua-resty-ffi C APIs.

* `ffi/echo.py`

Echo the request.

* `ffi/kafka.py`

Simple kafka client. Implements produce and consumer group.

## Build and test

```bash
# in one terminal
# start kafka container for test
docker compose up -d

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

```python
class State:
    def poll(self, tq):
        while True:
            # poll a task
            task = C.ngx_http_lua_ffi_task_poll(ffi.cast("void*", tq))
            # if the task queue is done, then quit
            if task == ffi.NULL:
                break
            # get the request from the task
            # It assumes the message in C string, so no need to get the request length
            req = C.ngx_http_lua_ffi_get_req(task, ffi.NULL)
            # copy the request as response, allocated by C strdup()
            # note that both request and response would be freed by nginx
            res = C.strdup(req)
            C.ngx_http_lua_ffi_respond(task, 0, res, 0)
        print("exit python echo runtime")

# implement an entry function
def init(cfg, tq):
    # launch a thread to poll tasks
    st = State()
    t = threading.Thread(target=st.poll, args=(tq,))
    t.daemon = True
    t.start()
    return 0
```

Specify the entry module and function in lua and use it:

```lua
-- note that the python loader library symbols must be opened in global
local demo = ngx.load_ffi("ffi_python3", "ffi.echo,init",
    {is_global = true, unpin = true})
local ok, res = demo:echo("hello")
assert(ok)
assert(res == "hello")

-- for python >= 3.8, you could use '?' as module suffix
-- to indicate that the module is hot-reload, i.e.
-- when the module gets loaded next time (after previous unload),
-- it would reload the module instead of the VM cached version
local demo = ngx.load_ffi("ffi_python3", "ffi.echo?,init",
    {is_global = true, unpin = true})
```
