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
typedef struct lws_function_s lws_function_t;


struct lws_monitor_s {
	ngx_atomic_t     states_n;         /* number of Lua states (active + inactive) */
	ngx_atomic_t     requests_n;       /* number of queued requests */
	ngx_atomic_t     memory_used;      /* used memory */
	ngx_atomic_t     request_count;    /* requests served */
	ngx_atomic_t     profiler;         /* profiler status; 0 = disabled, 1 = CPU, 2 = wall */
	ngx_int_t        out_of_memory;    /* out-of-memory; 0 = no */
	size_t           functions_n;      /* number of profiled functions */
	size_t           functions_alloc;  /* allocated profiled functions */
	lws_function_t  *functions;        /* profiled functions */
};

struct lws_function_s {
	ngx_str_t        key;         /* key */
	ngx_uint_t       calls;       /* number of calls */
	struct timespec  time_self;   /* self time */
	struct timespec  time_total;  /* total time */
	size_t           memory;      /* allocated memory */
};


char *lws_monitor(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


#define LWS_MONITOR_SIZE  (32 * 4096)


#endif /* _LWS_MONITOR_INCLUDED */
