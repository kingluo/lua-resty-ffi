# Python loader library

It uses [`cffi`](https://cffi.readthedocs.io/) to call lua-resty-ffi C APIs.

## Examples

* `ffi/echo.py` Minimal example, echo the request.
* `ffi/kafka.py` Simple kafka client. Implements produce and consumer group.
* [lua-resty-ffi-ldap](https://github.com/kingluo/lua-resty-ffi-ldap) ldap client, supports all SASL auth methods
* [lua-resty-ffi-graphql-resolver](https://github.com/kingluo/lua-resty-ffi-graphql-resolver) embed graphql server into nginx

## Install via luarocks

```bash
# install lua-resty-ffi and lua-resty-ffi-python
# https://github.com/kingluo/lua-resty-ffi#install-lua-resty-ffi-via-luarocks
# set `OR_SRC` to your openresty source path
luarocks config variables.OR_SRC /tmp/tmp.Z2UhJbO1Si/openresty-1.21.4.1
luarocks install lua-resty-ffi-python
```

## Build from source

```bash
# compile loader library
make
```

## Demo

```bash
# install latest docker version
# https://docs.docker.com/engine/install/

# start kafka container for test
docker compose up -d

# ensure kafka is ready
docker compose logs -f

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
local demo = ngx.load_ffi("resty_ffi_python", "ffi.echo,init",
    {is_global = true, unpin = true})
local ok, res = demo:echo("hello")
assert(ok)
assert(res == "hello")

-- for python >= 3.8, you could use '?' as module suffix
-- to indicate that the module is hot-reload, i.e.
-- when the module gets loaded next time (after previous unload),
-- it would reload the module instead of the VM cached version
local demo = ngx.load_ffi("resty_ffi_python", "ffi.echo?,init",
    {is_global = true, unpin = true})
```
