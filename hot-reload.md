## Configuration and Runtime

For one configuration, lua-resty-ffi will create one dedicated runtime.

That is, if the configuration is different, the runtime is different.

Runtimes are independent, even for the same library.

The runtime could be some threads, or coroutines, or mixed, depending on the language and library.

Let's recall the lua API:

`local runtime = ngx.load_ffi(lib, cfg, opts)`

`cfg`: configuration in any format, e.g. plain string, JSON, or null (sometimes we don't need configuration).

For Go and Rust, `lib` is the library you compiled for some usage. Different projects compiles out different library file.

For example, you could write an etcd client library and load it with some configuration:

```lua
-- here the configuration is the etcd server addresses, in JSON array
local demo = ngx.load_ffi("ffi_go_etcd", "[\"localhost:2379\"]")
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

This is done by appending a null message to the request queue of the runtime, which notifies the
poll thread of the runtime to cleanup everything.
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

Python has builtin module reloading API: [`PyImport_ReloadModule`](https://docs.python.org/3/c-api/import.html#c.PyImport_ReloadModule).

### Nodejs

All loaded modules are cached in [`require.cache`](https://nodejs.org/api/modules.html#requirecache),
so removing the cached version before loading accomplishes the hot-reload.

### Java

Each class is associated with its classloader.

In brief, writing an **anti-parent-delegation** classloader,
each time you load a class, you create a new classloader to load it,
then hot-reload is done.

Note that a java file may be compiled into multiple classes,
so hot-realod should happen at the package level of the java file.

### Go

For static and machine-code-compiled languages, it's not easy to do code hot-reload.

Each `dlopen()` will increase the reference counter of the shared library file handle,
while the `dlclose()` does the reverse.

When the reference counter becomes `0`, the file handle is removed from the process,
and later `dlopen()` will open a new handle from the filesystem.

So, the only opportunity to do code hot-reload is clear the reference counter.
However, the reality is more complicated. Some library runtime does not consider cleanup,
which would keep some resources bound to the file handle, e.g. `mmap` somthing from the
shared library, resist some threads in the background, etc.

Go is the typical example:

https://github.com/golang/go/issues/11100

So Go does not support code hot-reload, yet.

### Rust

Just like go, rust has the same issue.
But luckily, rust doesn't need to maintain a language-level runtime.

So, depending on the implementation of your rust shared library, code hot-reload is possible or not.

Examples:

* simple threading

Yes, it applies the `dlopen()` reference counting, so when all runtimes spawn from the same libraries are
unloaded, you could reload the library in the next loading.

You could pass `unpin` option to `ngx.load_ffi()` to achieve this goal:

```lua
local demo = ngx.load_ffi("echo", cjson.encode(cfg), {unpin = true})
```

* async rust based on [tokio](https://tokio.rs/)

No, tokio has complicated mechanism like go. Currently, code hot-reload is not supported.

### Notes about `unpin` option

By default, all shared libraries are disallowd to `dlclose()`, i.e. `unpin` is `false`.
That is, all shared libraries handles are cached forever.

Reasons:

* I haven't successfully destroy the Java VM, and it would fail to init the VM the next time.
That is, `(*vm)->DestroyJavaVM(vm)` does not work in my loader code. I will try to solve it in future.
* Go does not support code hot-reload.
* Rust partially supports code hot-reload.
* Python/Java/Nodejs already support code hot-reload in the module/class level. It's rare to reload
the VM itself.

Interestingly, Python applies perfect `dlopen()/dlclose()` rules, so with `unpin=true`, you could even
reload the python interpreter itself! For example, upgrade the python from 3.8 to 3.11 on-the-fly.
