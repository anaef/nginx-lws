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
	size_t           memory_monitor;  /* monitor memory */
	ngx_int_t        request_count;   /* requests served */
	ngx_msec_t       time_max;        /* maximum lifetime */
	ngx_msec_t       timeout;         /* idle timeout */
	ngx_event_t      tev;             /* time event */
	unsigned         in_use:1;        /* state in use */
	unsigned         init:1;          /* state initialized */
	unsigned         close:1;         /* state is to be closed */
	unsigned         profiler:2;      /* profiler status; 0 = disabled, 1 = CPU, 2 = wall */
};


lws_state_t *lws_get_state(lws_request_ctx_t *ctx);
void lws_put_state(lws_request_ctx_t *ctx, lws_state_t *state);
int lws_run_state(lws_request_ctx_t *ctx);
void lws_close_states(lws_loc_conf_t *llcf);


#endif /* _LWS_STATE_INCLUDED */
