## Configuration and Runtime

For one configuration, lua-resty-ffi will create one dedicated runtime.

That is, if the configuration is different, the runtime is different.

Runtimes are independent, even for the same library.

The runtime could be some threads, or coroutines, or mixed, depending on the language and library.

Let's recall the lua API:

`local runtime = ngx.load_ffi(lib, cfg, opts)`

`cfg`: configuration in any format, e.g. plain string, JSON.

For Go and Rust, `lib` is the library you compiled for some usage. Different job compiles different library file.

For example, you could write a etcd client library and load it with some configuration:

```lua
-- here the configuration is the etcd server addresses, in JSON array
local demo = ngx.load_ffi("ffi_go_etcd", "[\"localhost:2379\"]")

-- of course, the configuration could be empty
local demo = ngx.load_ffi("ffi_go_echo")
```

For Python, Java and Nodejs, the library is the fixed loader library, and the `cfg` is in csv format,
specifing the real module/class to do the init:

`<module/class>,<function>,<cfg>`

The last part of `cfg` is the real configuration just like you specify in Go and Rust.

```lua
-- python
-- libffi_python3.so
-- ffi/kafka.py
local demo = ngx.load_ffi("ffi_python3",
    [[ffi.kafka,init,{"servers":"localhost:9092", "topic":"foobar", "group_id": "foobar"}]],
    {is_global = true})

-- java
-- libffi_java.so
-- demo/http2/App.class
local demo = ngx.load_ffi("ffi_java", "demo.http2.App?,init,")

-- nodejs
-- libffi_nodejs.so
-- demo/http2.js
local demo = ngx.load_ffi("ffi_nodejs", "demo/http2?,init", {is_global=true})
```

## Unload

When the configuration is changed, we need to release the old configuration, i.e. the old runtime.

This is done by appending a null message to the request queue of the runtime.
The advantage is the shutdown happens after all pending messages get handled, i.e. graceful shutdown.
The disadvantage is it may makes some arbitrary time.

The lua API is:

`runtime:__unload()`

## Code Hot-Reload

How to apply code changes without reloading/restarting nginx?

For python, java and nodejs, if you place a question mark suffix in the `module/class`,
the loader library will reload the module/class but not the cached version.

For example:

```lua
local demo = ngx.load_ffi("ffi_python3", "ffi.echo?,init",
    {is_global = true, unpin = true})
```

Each time you call `ngx.load_ffi`, either with different configuration or after unloading the configuration,
the loader will load a fresh `ffi/echo.py` from the filesystem,
then the result runtime will reflect your code change of `ffi/echo.py`.

### Python

Python has builtin module reloading API: `PyImport_ReloadModule`.

### Nodejs

All loaded modules are cached in `require.cache`,
so removing the cached version before loading accomplishes the hot-reload.

### Java

Each class is associated with its classloader.
In brief, writing a anti-parent-delegation classloader,
each time you load a class, you create a new classloader to load it,
then hot-reload is done.

Note that a java file may be compiled into multiple classes,
so hot-realod should happen at the package level of the java file.

### Go

TODO

### Rust

TODO
