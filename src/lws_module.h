/*
 * LWS module
 *
 * Copyright (C) 2023 Andre Naef
 */


#ifndef _LWS_MODULE_INCLUDED
#define _LWS_MODULE_INCLUDED


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#define LWS_THREAD_POOL_NAME_DEFAULT    "default"
#define LWS_STAT_CACHE_CAP_DEFAULT      1024
#define LWS_STAT_CACHE_TIMEOUT_DEFAULT  30
#define lws_cpylit(p, lit)              ngx_cpymem(p, lit, sizeof(lit) - 1)


typedef struct lws_main_conf_s lws_main_conf_t;
typedef struct lws_loc_conf_s lws_loc_conf_t;
typedef struct lws_request_ctx_s lws_request_ctx_t;
typedef struct lws_variable_s lws_variable_t;


#include <lws_monitor.h>
#include <lws_state.h>
#include <lws_table.h>


typedef enum {
	LWS_FS_UNKNOWN,
	LWS_FS_FOUND,
	LWS_FS_NOT_FOUND
} lws_file_status_e;

typedef enum {
	LWS_ER_JSON,
	LWS_ER_HTML
} lws_error_response_e;

struct lws_main_conf_s {
	ngx_thread_pool_t  *thread_pool;         /* thread pool for async execution of Lua */
	ngx_str_t           thread_pool_name;    /* name of thread pool */
	lws_table_t        *stat_cache;          /* timed file stat cache to reduce syscalls */
	size_t              stat_cache_cap;      /* cap of stat cache; 0 = disabled */
	time_t              stat_cache_timeout;  /* timeout of stat cache */
	ngx_shm_zone_t     *monitor_shm;         /* monitor shared memory zone */
	ngx_slab_pool_t    *monitor_pool;        /* monitor slab allocator */
	lws_monitor_t      *monitor;             /* monitor */
};

struct lws_loc_conf_s {
	ngx_http_complex_value_t  *main;       /* filename of main Lua chunk */
	ngx_http_complex_value_t  *path_info;  /* path info */
	ngx_str_t    init;                     /* filename of init Lua chunk (runs once) */
	ngx_str_t    pre;                      /* filename of pre Lua chunk */
	ngx_str_t    post;                     /* filename of post Lua chunk */
	ngx_str_t    path;                     /* Lua path */
	ngx_str_t    cpath;                    /* Lua C path */
	size_t       states_max;               /* maximum Lua states; 0 = unrestricted */
	size_t       requests_max;             /* maximum queued requests; 0 = unrestricted */
	size_t       state_memory_max;         /* maximum Lua state memory; 0 = unrestricted */
	size_t       state_gc;                 /* Lua state explicit GC threshold; 0 = never */
	ngx_int_t    state_requests_max;       /* maximum Lua state requests; 0 = unlimited */
	ngx_msec_t   state_time_max;           /* maximum Lua state lifetime; 0 = unlimited */
	ngx_msec_t   state_timeout;            /* Lua state idle timeout; 0 = unlimited */
	ngx_uint_t   error_response;           /* error response [json, html] */
	ngx_flag_t   diagnostic;               /* include diagnostic w/ error response */
	ngx_flag_t   monitor;                  /* monitor enabled */
	ngx_array_t  variables;                /* variables */
	ngx_uint_t   states_n;                 /* number of Lua states (active + inactive) */
	ngx_queue_t  states;                   /* inactive Lua states */
	ngx_uint_t   requests_n;               /* number of queued requests */
	ngx_queue_t  requests;                 /* queued requests */
	ngx_event_t  qev;                      /* queue event */
};

struct lws_request_ctx_s {
	ngx_queue_t          queue;              /* location configuration queue */
	ngx_http_request_t  *r;                  /* NGINX HTTP request */
	ngx_str_t            main;               /* filename of main Lua chunk */
	ngx_str_t            path_info;          /* request path info */
	lws_state_t         *state;              /* active Lua state */
	lws_table_t         *variables;          /* request variables */
	lws_table_t         *request_headers;    /* request headers */
	FILE                *request_body;       /* HTTP request body stream */
	ngx_chain_t         *cl;                 /* HTTP request body chain */
	ngx_int_t            rc;                 /* NGINX response code */
	ngx_int_t            status;             /* HTTP reponse status */
	lws_table_t         *response_headers;   /* HTTP response headers */
	FILE                *response_body;      /* HTTP response body stream */
	ngx_str_t            response_body_str;  /* HTTP response body string */
	ngx_str_t            redirect;           /* NGINX internal redirect; @ prefix for name */
	ngx_str_t            redirect_args;      /* NGINX internal redirect args */
	ngx_str_t            diagnostic;         /* diagnostic response */
};

struct lws_variable_s {
	ngx_str_t   name;   /* variable name */
	ngx_int_t   index;  /* variable index */
};


extern ngx_module_t lws_module;


#endif /* _LWS_MODULE_INCLUDED */
