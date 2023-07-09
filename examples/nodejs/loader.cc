/*
 * Copyright (c) 2022, Jinhua Luo (kingluo) luajit.io@gmail.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "node.h"
#include "uv.h"
#include <assert.h>
#include <stdio.h>
#include <sys/socket.h>
#include <linux/un.h>
#include <unistd.h>

using node::CommonEnvironmentSetup;
using node::Environment;
using node::MultiIsolatePlatform;
using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Locker;
using v8::MaybeLocal;
using v8::V8;
using v8::Value;

static int client_fd;
static bool initialized = false;
static pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;

static void* thread_fn(void* data)
{
    int argc = 1;
    char _null[1];
    char* argvv = _null;
    char** argv = &argvv;
    argv = uv_setup_args(argc, argv);
    std::vector<std::string> args(argv, argv + argc);
    std::unique_ptr<node::InitializationResult> result =
        node::InitializeOncePerProcess(args, {
            node::ProcessInitializationFlags::kNoInitializeV8,
            node::ProcessInitializationFlags::kNoInitializeNodeV8Platform
        });

    for (const std::string& error : result->errors())
        fprintf(stderr, "%s: %s\n", args[0].c_str(), error.c_str());
    if (result->early_return() != 0) {
        fprintf(stderr, "early return: exit_code=%d\n", result->exit_code());
        return NULL;
    }

    std::unique_ptr<MultiIsolatePlatform> platform =
        MultiIsolatePlatform::Create(4);
    V8::InitializePlatform(platform.get());
    V8::Initialize();

    std::vector<std::string> errors;
    std::unique_ptr<CommonEnvironmentSetup> setup =
        CommonEnvironmentSetup::Create(platform.get(), &errors, result->args(), result->exec_args());
    if (!setup) {
        for (const std::string& err : errors)
            fprintf(stderr, "%s: %s\n", args[0].c_str(), err.c_str());
        return NULL;
    }

    Isolate* isolate = setup->isolate();
    Environment* env = setup->env();

    {
        Locker locker(isolate);
        Isolate::Scope isolate_scope(isolate);
        HandleScope handle_scope(isolate);
        Context::Scope context_scope(setup->context());

        MaybeLocal<Value> loadenv_ret = node::LoadEnvironment(
            env,
            "const publicRequire ="
            "  require('module').createRequire(process.env.NODE_PATH + '/');"
            "globalThis.require = publicRequire;"
            "require('vm').runInThisContext(\""
            "const net = require('net');"
            "const fs = require('fs');"
            "const path = '/tmp/.resty_ffi_nodejs_loader.' + process.pid;"
            "try { fs.unlinkSync(path) } catch(err) {}"
            "const unixSocketServer = net.createServer();"
            "unixSocketServer.listen(path, () => {"
            "  console.log('ffi nodejs listening');"
            "});"
            "unixSocketServer.on('connection', (s) => {"
            "  s.on('data', (data) => {"
            "    try {"
            "      const arr = data.toString().split(',');"
            "      const tq = BigInt(arr[0]);"
            "      var mod = arr[1];"
            "      const func = arr[2];"
            "      const cfg = '';"
            "      if (arr.length == 4) {"
            "        cfg = arr[3];"
            "      }"
            "      if (mod.endsWith('?')) {"
            "        mod = mod.slice(0, -1);"
            "        var mp = require.resolve(mod);"
            "        if (require.cache[mp]) {"
            "          delete require.cache[mp];"
            "          console.log(`[clear] module: ${mp}`);"
            "        }"
            "      }"
            "      var rc = require(mod)[func](cfg, tq);"
            "      s.write(rc.toString());"
            "    } catch(err) {"
            "      console.log(err);"
            "      s.write('1');"
            "      return;"
            "    }"
            "  });"
            "});"
            "\");"
            );

        if (loadenv_ret.IsEmpty())  // There has been a JS exception.
            return NULL;

        node::SpinEventLoop(env).FromMaybe(1);

        node::Stop(env);
    }

    V8::Dispose();
    V8::DisposePlatform();

    node::TearDownOncePerProcess();

    return NULL;
}

extern "C" int libffi_init(char* cfg, void *tq)
{
    pthread_mutex_lock(&init_lock);

    if (!initialized) {
        initialized = true;

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        pthread_t tid;
        pthread_create(&tid, &attr, thread_fn, NULL);
        sleep(1);

        // connect nodejs
        if ((client_fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
            perror("socket");
        }
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        snprintf(addr.sun_path, UNIX_PATH_MAX, "/tmp/.resty_ffi_nodejs_loader.%d", getpid());
        for (int i = 0; i < 10; i++) {
            if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
                perror("connect");
                sleep(1);
            } else {
                break;
            }
        }
    }

    int ret = 0;
    char buf[8192];
    memset(buf, 0, 8192);
    sprintf(buf, "%p,%s", tq, cfg);
    if (send(client_fd, buf, strlen(buf), 0) == -1) {
        perror("send");
    }

    int len;
    memset(buf, 0, 8192);
    if ((len = recv(client_fd, buf, 8192, 0)) < 0) {
        perror("recv");
    }
    sscanf(buf, "%d", &ret);

    pthread_mutex_unlock(&init_lock);
    return ret;
}
