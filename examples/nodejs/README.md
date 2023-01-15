# Node.js ffi library

* `demo/echo.py`

Echo the request.

* `demo/http2.py`

Simple http2 client.

## Background

Check my blog for detail:

http://luajit.io/post/use-nodejs-to-extend-openresty/

## Build and install Node.js

Node.js doesn't provide shared library officially,
so we need to build it from source.

Note that we need to use `--shared-openssl` option to use system openssl,
otherwise the embedding into openresty would crash on openssl APIs
(the reason is unknown yet).

And, by default, Node.js builds a lot of libraries with static linking,
so the build consumes a long time, then it's better to run `make -j<N>` where
`<N>` is the number of CPU cores to speed up the build process.

Below steps build and install Node.js into `/usr/local`.

```bash
apt install libssl-dev

cd /opt
git clone https://github.com/nodejs/node
cd /opt/node

git checkout v19.2.0

./configure --shared --shared-openssl --debug-node --gdb
make -j2 install
ln -sf libnode.so.111 /usr/local/lib/libnode.so
```

## Build and test

```bash
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

```js
const { Worker, isMainThread, parentPort } = require('node:worker_threads');
const ffi = require("resty_ffi")

if (isMainThread) {
  // main thread logic
  // define a module method to do init
  exports.init = (cfg, tq) => {
    // create a worker thread
    // note that the worker thread also requires this module
    var worker = new Worker(__filename);
    // get a new task from nginx
    worker.on('message', (tsk) => {
      const req = ffi.get_req(tsk)
      //
      // handle request here...
      //
      ffi.respond(tsk, 0, req)
    });
    // tell the worker thread to do polling
    worker.postMessage(tq);
    return 0;
  };
} else {
  // worker thread logic
  parentPort.once('message', (tq) => {
    while (true) {
      // poll task from nginx
      const tsk = ffi.poll_task(tq)
      // if task queue is done, i.e. __unload() by lua, then exit
      if (tsk == 0) {
        console.log('exit ffi nodejs echo runtime');
        break;
      }
      // notify the main thread to handle the request
      parentPort.postMessage(tsk);
    }
  });
}
```

Specify the entry module and function in lua and use it:

```lua
-- note that the nodejs loader library symbols must be opened in global
local demo = ngx.load_ffi("ffi_nodejs", "demo/echo,init", {is_global=true})
local ok, res = demo:echo("hello")
assert(ok)
assert(res == "hello")

-- You could use '?' as module suffix to indicate that the module is hot-reload.
-- When the module gets loaded next time (after previous unload),
-- it would reload the module instead of the VM cached version
local demo = ngx.load_ffi("ffi_nodejs", "demo/echo?,init", {is_global=true})
```
