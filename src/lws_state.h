/*
 * LWS state
 *
 * Copyright (C) 2023 Andre Naef
 */


#ifndef _LWS_STATE_INCLUDED
#define _LWS_STATE_INCLUDED


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <lua.h>
#include <lws_module.h>


/* LWS state */
typedef struct lws_state_s lws_state_t;
struct lws_state_s {
	ngx_queue_t  queue;
	lua_State   *L;
	size_t       memory_used;
	size_t       memory_limit;
	ngx_int_t    lifecycles;
	unsigned     init:1;
	unsigned     error:1;
};


lws_state_t *lws_create_state(ngx_http_request_t *r);
void lws_close_state(lws_state_t *state, ngx_log_t *log);
lws_state_t *lws_get_state(ngx_http_request_t *r);
void lws_put_state(ngx_http_request_t *r, lws_state_t *state);
int lws_run_state(lws_request_ctx_t *ctx);


#endif /* _LWS_STATE_INCLUDED */
