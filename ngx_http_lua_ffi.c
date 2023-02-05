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

#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"


#include "ngx_http_lua_common.h"
#include "ngx_http_lua_util.h"


#if defined(NGX_THREADS) && defined(NGX_HAVE_EVENTFD)


#include <ngx_thread_pool.h>

#ifndef SHARED_OBJECT

extern void ngx_thread_pool_notify_inject(void *task);

#else

#include <stdlib.h>

#include <ngx_core.h>

#include "symbols.h"
#include "build-id.h"

typedef struct {
    ngx_thread_task_t        *first;
    ngx_thread_task_t       **last;
} ngx_thread_pool_queue_t;

static ngx_atomic_t* ngx_thread_pool_done_lock;
static ngx_thread_pool_queue_t* ngx_thread_pool_done;
static ngx_event_handler_pt ngx_thread_pool_handler;

int
lua_resty_ffi_init()
{
    // check if build-id matches
    const struct build_id_note *note = build_id_find_nhdr_by_name("");
    if (!note)
        return -1;

    ElfW(Word) len = build_id_length(note);

    const uint8_t *build_id = build_id_data(note);

    char* buf = (char*)calloc(len*2, 1);
    for (ElfW(Word) i = 0; i < len; i++) {
        sprintf(buf + strlen(buf), "%02x", build_id[i]);
    }

    if (strcmp(buf, NGX_BUILD_ID) != 0) {
        printf("build-id mismatch, expected=%s, actual=%s\n", buf, NGX_BUILD_ID);
        free(buf);
        return -2;
    }

    // resolve symbols
    char* addr;
    addr = (char*)&ngx_cycle;
    addr += NGX_THREAD_POOL_DONE_OFFSET;
    ngx_thread_pool_done = (ngx_thread_pool_queue_t*)addr;

    addr = (char*)&ngx_cycle;
    addr += NGX_THREAD_POOL_DONE_LOCK_OFFSET;
    ngx_thread_pool_done_lock = (ngx_atomic_t*)addr;

    addr = (char*)&ngx_calloc;
    addr += NGX_THREAD_POOL_HANDLER_OFFSET;
    ngx_thread_pool_handler = (ngx_event_handler_pt)addr;

    free(buf);

    return 0;
}

static void
ngx_thread_pool_notify_inject(void *data)
{
    ngx_thread_task_t *task = data;

    task->next = NULL;

    ngx_spinlock(ngx_thread_pool_done_lock, 1, 2048);

    *((*ngx_thread_pool_done).last) = task;
    (*ngx_thread_pool_done).last = &task->next;

    ngx_memory_barrier();

    ngx_unlock(ngx_thread_pool_done_lock);

    (void) ngx_notify(ngx_thread_pool_handler);
}

#endif


typedef struct {
    ngx_http_lua_co_ctx_t   *wait_co_ctx;
    int                      rc;
    char                    *req;
    int                      req_len;
    char                    *rsp;
    int                      rsp_len;
    int                      is_abort:1;
} ffi_task_ctx_t;


typedef struct {
    ngx_thread_task_t        *first;
    ngx_thread_task_t       **last;
} ngx_task_queue_t;

#define ngx_task_queue_init(q) \
    (q)->first = NULL;         \
    (q)->last = &(q)->first


typedef struct {
    ngx_thread_mutex_t        mtx;
    ngx_task_queue_t          queue;
    ngx_int_t                 waiting;
    ngx_thread_cond_t         cond;
    ngx_log_t                *log;
    ngx_int_t                 max_queue;
} ffi_task_queue_t;


char*
ngx_http_lua_ffi_get_req(void *tsk, int *len)
{
    ngx_thread_task_t *task = tsk;
    ffi_task_ctx_t *ctx = task->ctx;
    if (len != NULL) {
        *len = ctx->req_len;
    }
    return ctx->req;
}


void
ngx_http_lua_ffi_respond(void *tsk, int rc, char* rsp, int rsp_len)
{
    ngx_thread_task_t *task = tsk;
    ffi_task_ctx_t *ctx = task->ctx;
    ctx->rc = rc;
    ctx->rsp = rsp;
    ctx->rsp_len = rsp_len;
    ngx_thread_pool_notify_inject(task);
}


/*
 * Re-implement ngx_thread_task_alloc to avoid alloc from request pool
 * since the request may exit before worker thread finish.
 * And we may implement a memory pool for this allocation in the future
 * to avoid memory fragmentation.
 */
static ngx_thread_task_t *
ngx_http_lua_ffi_task_alloc(size_t size)
{
    ngx_thread_task_t  *task;

    task = ngx_calloc(sizeof(ngx_thread_task_t) + size, ngx_cycle->log);
    if (task == NULL) {
        return NULL;
    }

    task->ctx = task + 1;

    return task;
}


static void
ngx_http_lua_ffi_task_free(ffi_task_ctx_t *ctx)
{
    if (ctx->req) {
        free(ctx->req);
    }
    if (ctx->rsp) {
        free(ctx->rsp);
    }
    ngx_thread_task_t *task = (void*)ctx;
    ngx_free(task - 1);
}


static ngx_int_t
ngx_http_lua_ffi_resume(ngx_http_request_t *r)
{
    lua_State                   *vm;
    ngx_connection_t            *c;
    ngx_int_t                    rc;
    ngx_uint_t                   nreqs;
    ngx_http_lua_ctx_t          *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_lua_module);
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    ctx->resume_handler = ngx_http_lua_wev_handler;

    c = r->connection;
    vm = ngx_http_lua_get_lua_vm(r, ctx);
    nreqs = c->requests;

    rc = ngx_http_lua_run_thread(vm, r, ctx, 3);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "lua run thread returned %d", rc);

    if (rc == NGX_AGAIN) {
        return ngx_http_lua_run_posted_threads(c, vm, r, ctx, nreqs);
    }

    if (rc == NGX_DONE) {
        ngx_http_lua_finalize_request(r, NGX_DONE);
        return ngx_http_lua_run_posted_threads(c, vm, r, ctx, nreqs);
    }

    /* rc == NGX_ERROR || rc >= NGX_OK */

    if (ctx->entered_content_phase) {
        ngx_http_lua_finalize_request(r, rc);
        return NGX_DONE;
    }

    return rc;
}


/* executed in nginx event loop */
static void
ngx_http_lua_ffi_event_handler(ngx_event_t *ev)
{
    ffi_task_ctx_t         *ffi_ctx;
    lua_State              *L;
    ngx_http_request_t     *r;
    ngx_connection_t       *c;
    ngx_http_lua_ctx_t     *ctx;

    ffi_ctx = ev->data;

    if (ffi_ctx->is_abort) {
        goto failed;
    }

    L = ffi_ctx->wait_co_ctx->co;

    r = ngx_http_lua_get_req(L);
    if (r == NULL) {
        goto failed;
    }

    c = r->connection;

    ctx = ngx_http_get_module_ctx(r, ngx_http_lua_module);
    if (ctx == NULL) {
        goto failed;
    }

    lua_pushboolean(L, ffi_ctx->rc ? 0 : 1);

    if (ffi_ctx->rc) {
        lua_pushinteger(L, ffi_ctx->rc);
    }

    if (ffi_ctx->rsp) {
        if (ffi_ctx->rsp_len) {
            lua_pushlstring(L, ffi_ctx->rsp, ffi_ctx->rsp_len);
        } else {
            lua_pushstring(L, ffi_ctx->rsp);
        }
    } else {
        lua_pushnil(L);
    }

    if (!ffi_ctx->rc) {
        lua_pushnil(L);
    }

    ctx->cur_co_ctx = ffi_ctx->wait_co_ctx;
    ctx->cur_co_ctx->cleanup = NULL;

    ngx_http_lua_ffi_task_free(ffi_ctx);

    /* resume the caller coroutine */

    if (ctx->entered_content_phase) {
        (void) ngx_http_lua_ffi_resume(r);

    } else {
        ctx->resume_handler = ngx_http_lua_ffi_resume;
        ngx_http_core_run_phases(r);
    }

    ngx_http_run_posted_requests(c);

    return;

failed:

    ngx_http_lua_ffi_task_free(ffi_ctx);
    return;
}


static void
ngx_http_lua_ffi_cleanup(void *data)
{
    ngx_http_lua_co_ctx_t *ctx = data;
    ffi_task_ctx_t *ffi_ctx = ctx->data;
    ffi_ctx->is_abort = 1;
}


void*
ngx_http_lua_ffi_create_task_queue(int max_queue)
{
    ffi_task_queue_t* tp = ngx_calloc(sizeof(ffi_task_queue_t), ngx_cycle->log);

    ngx_task_queue_init(&tp->queue);

    if (ngx_thread_mutex_create(&tp->mtx, ngx_cycle->log) != NGX_OK) {
        ngx_free(tp);
        return NULL;
    }

    if (ngx_thread_cond_create(&tp->cond, ngx_cycle->log) != NGX_OK) {
        (void) ngx_thread_mutex_destroy(&tp->mtx, ngx_cycle->log);
        ngx_free(tp);
        return NULL;
    }

    tp->log = ngx_cycle->log;
    tp->max_queue = max_queue;

    return tp;
}


static void
ngx_http_lua_ffi_free_task_queue(ffi_task_queue_t* tp)
{
    /*
     * After special finished task, no new task post (ensured by lua land),
     * and all pending tasks are handled, so no tasks in the queue now.
     */
    ngx_thread_mutex_destroy(&tp->mtx, ngx_cycle->log);
    ngx_thread_cond_destroy(&tp->cond, ngx_cycle->log);
    ngx_free(tp);
}


static ngx_thread_task_t*
ngx_http_lua_ffi_create_task(ngx_http_request_t *r)
{
    ngx_http_lua_ctx_t     *ctx;
    ngx_thread_task_t      *task;
    ffi_task_ctx_t         *ffi_ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_lua_module);
    if (ctx == NULL) {
        return NULL;
    }

    task = ngx_http_lua_ffi_task_alloc(sizeof(ffi_task_ctx_t));

    if (task == NULL) {
        return NULL;
    }

    ffi_ctx = task->ctx;

    ffi_ctx->wait_co_ctx = ctx->cur_co_ctx;

    ctx->cur_co_ctx->cleanup = ngx_http_lua_ffi_cleanup;
    ctx->cur_co_ctx->data = ffi_ctx;

    task->event.handler = ngx_http_lua_ffi_event_handler;
    task->event.data = ffi_ctx;

    return task;
}


static ngx_int_t
ngx_task_post(ffi_task_queue_t *tp, ngx_thread_task_t *task)
{
    if (task->event.active) {
        ngx_log_error(NGX_LOG_ALERT, tp->log, 0,
                      "task #%ui already active", task->id);
        return NGX_ERROR;
    }

    if (ngx_thread_mutex_lock(&tp->mtx, tp->log) != NGX_OK) {
        return NGX_ERROR;
    }

    if (task->ctx && tp->waiting >= tp->max_queue) {
        (void) ngx_thread_mutex_unlock(&tp->mtx, tp->log);

        ngx_log_error(NGX_LOG_ERR, tp->log, 0,
                      "queue %p overflow: %i tasks waiting",
                      tp, tp->waiting);
        return NGX_ERROR;
    }

    task->event.active = 1;

    task->id = 0;
    task->next = NULL;

    if (ngx_thread_cond_signal(&tp->cond, tp->log) != NGX_OK) {
        (void) ngx_thread_mutex_unlock(&tp->mtx, tp->log);
        return NGX_ERROR;
    }

    *tp->queue.last = task;
    tp->queue.last = &task->next;

    tp->waiting++;

    (void) ngx_thread_mutex_unlock(&tp->mtx, tp->log);

    return NGX_OK;
}


int
ngx_http_lua_ffi_task_post(void *p0, void* p, char* req, int req_len)
{
    ngx_http_request_t *r = p0;
    ffi_task_queue_t *tp = p;
    ngx_thread_task_t *task = ngx_http_lua_ffi_create_task(r);
    ffi_task_ctx_t *ctx = task->ctx;
    ctx->req = req;
    ctx->req_len = req_len;
    if (ngx_task_post(tp, task) != NGX_OK) {
        ngx_http_lua_ffi_task_free(ctx);
        return NGX_ERROR;
    }
    return NGX_OK;
}


int
ngx_http_lua_ffi_task_finish(void* p)
{
    ffi_task_queue_t *tp = p;

    ngx_thread_task_t *task = ngx_calloc(sizeof(ngx_thread_task_t), ngx_cycle->log);

    /* append, so that all pending tasks get handled before finished */
    if (ngx_task_post(tp, task) != NGX_OK) {
        ngx_free(task);
        return NGX_ERROR;
    }

    return NGX_OK;
}


void*
ngx_http_lua_ffi_task_poll(void *p)
{
    ffi_task_queue_t *tp = p;
    ngx_thread_task_t *task = NULL;
    if (ngx_thread_mutex_lock(&tp->mtx, tp->log) != NGX_OK) {
        return NULL;
    }

    /* the number may become negative */
    tp->waiting--;

    while (tp->queue.first == NULL) {
        if (ngx_thread_cond_wait(&tp->cond, &tp->mtx, tp->log)
                != NGX_OK)
        {
            (void) ngx_thread_mutex_unlock(&tp->mtx, tp->log);
            return NULL;
        }
    }

    task = tp->queue.first;
    tp->queue.first = task->next;

    if (tp->queue.first == NULL) {
        tp->queue.last = &tp->queue.first;
    }

    if (ngx_thread_mutex_unlock(&tp->mtx, tp->log) != NGX_OK) {
        return NULL;
    }

    if (task->ctx) {
        return task;
    }

    ngx_free(task);
    ngx_http_lua_ffi_free_task_queue(tp);
    return NULL;
}


#endif

/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
