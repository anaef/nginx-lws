/*
 * LWS monitor
 *
 * Copyright (C) 2023 Andre Naef
 */


#ifndef _LWS_MONITOR_INCLUDED
#define _LWS_MONITOR_INCLUDED


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct lws_monitor_s lws_monitor_t;

struct lws_monitor_s {
	ngx_atomic_t  states_n;       /* number of Lua states (active + inactive) */
	ngx_atomic_t  requests_n;     /* number of queued requests */
	ngx_atomic_t  memory_used;    /* used memory */
	ngx_atomic_t  request_count;  /* requests served */
	ngx_atomic_t  profiling;      /* profiler status; 0 = disabled */
};


char *lws_monitor(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


#endif /* _LWS_MONITOR_INCLUDED */