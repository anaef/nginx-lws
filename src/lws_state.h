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
	ngx_queue_t      queue;        /* location configuration queue */
	lws_loc_conf_t  *llcf;         /* location configuration */
	lua_State       *L;            /* Lua state */
	size_t           used_memory;  /* used memory */
	size_t           max_memory;   /* maximum memory */
	ngx_int_t        requests;     /* requests served */
	ngx_msec_t       max_time;     /* maximum lifetime */
	ngx_msec_t       timeout;      /* idle timeout */
	ngx_event_t      tev;          /* time event */
	unsigned         in_use:1;     /* state in use */
	unsigned         init:1;       /* state initialized */
	unsigned         close:1;      /* state is to be closed */
};


lws_state_t *lws_create_state(lws_request_ctx_t *ctx);
void lws_close_state(lws_state_t *state, ngx_log_t *log);
lws_state_t *lws_get_state(lws_request_ctx_t *ctx);
void lws_put_state(lws_request_ctx_t *ctx, lws_state_t *state);
int lws_run_state(lws_request_ctx_t *ctx);


#endif /* _LWS_STATE_INCLUDED */
