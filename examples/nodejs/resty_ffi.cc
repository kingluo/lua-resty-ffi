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

#include <node_api.h>
#include <stdlib.h>

extern "C" {
void* ngx_http_lua_ffi_task_poll(void *p);
char* ngx_http_lua_ffi_get_req(void *tsk, int *len);
void ngx_http_lua_ffi_respond(void *tsk, int rc, char* rsp, int rsp_len);
}

namespace resty_ffi {
napi_value poll_task(napi_env env, napi_callback_info args) {
    size_t argc = 1;
    napi_value argv[1];
    napi_status status = napi_get_cb_info(env, args, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc != 1) {
        napi_throw_type_error(env, NULL, "Wrong number of arguments.");
        return NULL;
    }

    uint64_t ptr = 0;
    bool lossless;
    napi_get_value_bigint_uint64(env, argv[0], &ptr, &lossless);
    (void)lossless;

    void* tq = (void*)ptr;
    uint64_t tsk = (uint64_t)ngx_http_lua_ffi_task_poll(tq);

    napi_value ret;
    napi_create_bigint_uint64(env, tsk, &ret);
    return ret;
}

napi_value get_req(napi_env env, napi_callback_info args) {
    size_t argc = 1;
    napi_value argv[1];
    napi_status status = napi_get_cb_info(env, args, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc != 1) {
        napi_throw_type_error(env, NULL, "Wrong number of arguments.");
        return NULL;
    }

    uint64_t ptr = 0;
    bool lossless;
    napi_get_value_bigint_uint64(env, argv[0], &ptr, &lossless);
    (void)lossless;

    void* tsk = (void*)ptr;
    int len;
    char* req = ngx_http_lua_ffi_get_req(tsk, &len);
    if (req) {
        napi_value ret;
        status = napi_create_string_utf8(env, req, len, &ret);
        if (status != napi_ok) return nullptr;
        return ret;
    }

    return nullptr;
}

napi_value respond(napi_env env, napi_callback_info args) {
    size_t argc = 3;
    napi_value argv[3];
    napi_status status = napi_get_cb_info(env, args, &argc, argv, NULL, NULL);
    if (status != napi_ok || (argc != 2 && argc != 3)) {
        napi_throw_type_error(env, NULL, "Wrong number of arguments.");
        return NULL;
    }

    uint64_t ptr = 0;
    bool lossless;
    napi_get_value_bigint_uint64(env, argv[0], &ptr, &lossless);
    (void)lossless;
    void* tsk = (void*)ptr;

    int32_t rc = 0;
    napi_get_value_int32(env, argv[1], &rc);

    char* buf = NULL;
    int len = 0;
    if (argc == 3) {
        size_t str_size;
        size_t len;
        napi_get_value_string_utf8(env, argv[2], NULL, 0, &str_size);
        buf = (char*)calloc(str_size + 1, sizeof(char));
        str_size = str_size + 1;
        napi_get_value_string_utf8(env, argv[2], buf, str_size, &len);
    }

    ngx_http_lua_ffi_respond(tsk, rc, buf, len);

    return nullptr;
}

napi_value init(napi_env env, napi_value exports) {
    napi_status status;
    napi_value fn;

    status = napi_create_function(env, nullptr, 0, poll_task, nullptr, &fn);
    if (status != napi_ok) return nullptr;

    status = napi_set_named_property(env, exports, "poll_task", fn);
    if (status != napi_ok) return nullptr;

    status = napi_create_function(env, nullptr, 0, get_req, nullptr, &fn);
    if (status != napi_ok) return nullptr;

    status = napi_set_named_property(env, exports, "get_req", fn);
    if (status != napi_ok) return nullptr;

    status = napi_create_function(env, nullptr, 0, respond, nullptr, &fn);
    if (status != napi_ok) return nullptr;

    status = napi_set_named_property(env, exports, "respond", fn);
    if (status != napi_ok) return nullptr;

    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init)
}  // namespace resty_ffi
