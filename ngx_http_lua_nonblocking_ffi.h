/*
 * Copyright (C) Jinhua Luo (kingluo)
 * I hereby assign copyright in this code to the lua-nginx-module project,
 * to be licensed under the same terms as the rest of the code.
 */

#ifndef _NGX_HTTP_LUA_NONBLOCKING_FFI_H_INCLUDED_
#define _NGX_HTTP_LUA_NONBLOCKING_FFI_H_INCLUDED_


void* ngx_nonblocking_ffi_create_task_queue(int max_queue);
int ngx_http_lua_nonblocking_ffi_task_post(void *r, void* p, char* req, int req_len);


void* ngx_http_lua_nonblocking_ffi_task_poll(void *p);
char* ngx_http_lua_nonblocking_ffi_get_req(void *tsk, int *len);
void ngx_http_lua_nonblocking_ffi_respond(void *tsk, int rc, char* rsp, int rsp_len);


#endif /* _NGX_HTTP_LUA_NONBLOCKING_FFI_H_INCLUDED_ */

/* vi:set ft=c ts=4 sw=4 et fdm=marker: */
