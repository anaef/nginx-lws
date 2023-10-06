/*
 * LWS profiler
 *
 * Copyright (C) 2023 Andre Naef
 */


#ifndef _LWS_PROFILER_INCLUDED
#define _LWS_PROFILER_INCLUDED


#include <ngx_config.h>
#include <lua.h>
#include <lws_table.h>


#define LWS_PROFILER             "lws.profiler"           /* profiler metatable */
#define LWS_PROFILER_CURRENT     "lws.profiler_current"   /* current profiler*/
#define LWS_PROFILER_KEY_MAX     256                      /* maximum length of function key */
#define LWS_PROFILER_CLOCK_CPU   CLOCK_THREAD_CPUTIME_ID  /* CPU profiler clock */
#define LWS_PROFILER_CLOCK_WALL  CLOCK_MONOTONIC_RAW      /* wall profiller clock */


typedef struct lws_profiler_s lws_profiler_t;
typedef struct lws_activation_record_s lws_activation_record_t;

struct lws_profiler_s {
	ngx_log_t                 *log;          /* log */
	lws_table_t               *functions;    /* functions */
	size_t                     stack_n;      /* stack count */
	size_t                     stack_alloc;  /* stack allocated */
	lws_activation_record_t  **stack;        /* stack */
	unsigned int               state;        /* state; 0 = disabled, 1 = CPU, 2 = wall */
	clockid_t                  clock;        /* clock */
};

struct lws_activation_record_s {
	ngx_uint_t       depth;             /* call depth */
	ngx_uint_t       calls;             /* number of calls */
	struct timespec  time_self_start;   /* start time for self time */
	struct timespec  time_self;         /* self time */
	struct timespec  time_total_start;  /* start time for total time */
	struct timespec  time_total;        /* total time */
	size_t           memory_start;      /* start memory */
	size_t           memory;            /* allocated memory */
};


int lws_open_profiler(lua_State *L);
int lws_start_profiler(lua_State *L);
int lws_stop_profiler(lua_State *L);


#endif /* _LWS_PROFILER_INCLUDED */
