/*
 * LWS profiler
 *
 * Copyright (C) 2023 Andre Naef
 */


#ifndef _LWS_PROFILER_INCLUDED
#define _LWS_PROFILER_INCLUDED


#include <lua.h>
#include <lws_table.h>


typedef struct lws_profiler_s lws_profiler_t;
typedef struct lws_activation_record_s lws_activation_record_t;

struct lws_profiler_s {
	lws_table_t               *functions;    /* functions */
	lws_activation_record_t  **stack;        /* stack */
	size_t                     stack_n;      /* stack count */
	size_t                     stack_alloc;  /* stack allocated */
	clockid_t                  clock_id;     /* clock */
};

struct lws_activation_record_s {
	ngx_uint_t       depth;             /* call depth */
	ngx_uint_t       calls;             /* number of calls */
	struct timespec  time_self_start;   /* start time for self time */
	struct timespec  time_total_start;  /* start time for total time */
	struct timespec  time_self;         /* self time */
	struct timespec  time_total;        /* total time */
	size_t           memory_start;      /* start memory */
	size_t           memory;            /* allocated memory */
};


int lws_profiler_open(lua_State *L);
int lws_profiler_start(lua_State *L);
int lws_profiler_stop(lua_State *L);


#define LWS_PROFILER          "lws.profiler"          /* profiler metatable */
#define LWS_PROFILER_CURRENT  "lws.profiler_current"  /* current profiler*/
#define LWS_PROFILER_KEY_MAX  256                     /* maximum length of function key */


#endif /* _LWS_PROFILER_INCLUDED */
