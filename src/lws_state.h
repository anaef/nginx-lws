/*
 * LWS state
 *
 * Copyright (C) 2023 Andre Naef
 */


#ifndef _LWS_STATE_INCLUDED
#define _LWS_STATE_INCLUDED


#include <lua.h>
#include <ngx_config.h>
#include <ngx_core.h>


typedef struct lws_state_s lws_state_t;


#include <lws_module.h>


struct lws_state_s {
	ngx_queue_t      queue;           /* location configuration queue */
	lws_loc_conf_t  *llcf;            /* location configuration */
	lua_State       *L;               /* Lua state */
	size_t           memory_used;     /* used memory */
	size_t           memory_max;      /* maximum memory */
	size_t           memory_monitor;  /* memory accounted for in monitor */
	ngx_int_t        request_count;   /* requests served */
	ngx_msec_t       time_max;        /* maximum lifetime */
	ngx_msec_t       timeout;         /* idle timeout */
	ngx_event_t      tev;             /* time event */
	unsigned         in_use:1;        /* state in use */
	unsigned         init:1;          /* state initialized */
	unsigned         close:1;         /* state is to be closed */
	unsigned         profiler:2;      /* profiler state; 0 = disabled, 1 = CPU, 2 = wall */
};


void lws_close_state(lws_state_t *state, ngx_log_t *log);
int lws_acquire_state(lws_request_ctx_t *ctx);
void lws_release_state(lws_request_ctx_t *ctx);
int lws_run_state(lws_request_ctx_t *ctx);


#endif /* _LWS_STATE_INCLUDED */
