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


typedef struct lws_main_conf_s lws_main_conf_t;
typedef struct lws_loc_conf_s lws_loc_conf_t;
typedef struct lws_request_ctx_s lws_request_ctx_t;
typedef struct lws_variable_s lws_variable_t;


#include <lws_table.h>
#include <lws_http.h>
#include <lws_state.h>
#include <lws_lib.h>


struct lws_main_conf_s {
	ngx_thread_pool_t  *thread_pool;          /* thread pool for async execution of Lua */
	ngx_str_t           thread_pool_name;     /* name of thread pool */
	lws_table_t        *stat_cache;           /* timed file stat cache to reduce syscalls */
	size_t              stat_cache_cap;       /* cap of stat cache; 0 = disabled */
	time_t              stat_cache_timeout;   /* timeout of stat cache */
};

struct lws_loc_conf_s {
	ngx_http_complex_value_t  *main;            /* filename of main chunk */
	ngx_http_complex_value_t  *path_info;       /* sub-path arguments */
	ngx_str_t                  init;            /* filename of init chunk (run once) */
	ngx_str_t                  pre;             /* filename of pre chunk */
	ngx_str_t                  post;            /* filename of post chunk */
	ngx_str_t                  path;            /* Lua path */
	ngx_str_t                  cpath;           /* Lua C path */
	size_t                     max_memory;      /* maximum memory; 0 = unrestricted */
	ngx_int_t                  max_requests;    /* maximum requests; 0 = unlimitted */
	ngx_msec_t                 max_time;        /* maximum lifetime; 0 = unlimited */
	ngx_msec_t                 timeout;         /* idle timeout; 0 = unlimited */
	size_t                     gc;              /* explicit gc; 0 = never */
	ngx_uint_t                 error_response;  /* error response [json, html] */
	ngx_flag_t                 diagnostic;      /* include diagnostic w/ error response */
	ngx_array_t                variables;       /* variables */
	ngx_queue_t                states;          /* inactive Lua states */
};

struct lws_request_ctx_s {
	ngx_http_request_t  *r;                   /* NGINX HTTP request */
	ngx_str_t            main;                /* filename of main chunk */
	ngx_str_t            path_info;           /* sub-path arguments */
	lws_loc_conf_t      *llcf;                /* location configuration */
	lws_state_t         *state;               /* active Lua state */
	lws_table_t         *variables;           /* request variables */
	lws_table_t         *request_headers;     /* request headers */
	FILE                *request_body;        /* HTTP request body stream */
	ngx_chain_t         *cl;                  /* HTTP request body chain */
	u_char              *pos;                 /* HTTP request body position */
	ngx_int_t            rc;                  /* NGINX response code */
	ngx_int_t            status;              /* HTTP reponse status */
	lws_table_t         *response_headers;    /* HTTP response headers */
	FILE                *response_body;       /* HTTP response body stream */
	ngx_str_t            response_body_str;   /* HTTP response body string */
	ngx_str_t           *redirect;            /* NGINX internal redirect; @ prefix for name */
	ngx_str_t           *redirect_args;       /* NGINX internal uri redirect args */
	ngx_str_t           *diagnostic;          /* diagnostic response */
	unsigned             complete:1;          /* request is complete */
};

struct lws_variable_s {
	ngx_str_t   name;    /* variable name */
	ngx_int_t   index;   /* variable index */
};

typedef enum {
	LWS_FS_UNKNOWN,
	LWS_FS_FOUND,
	LWS_FS_NOT_FOUND
} lws_file_status_e;

typedef enum {
	LWS_ER_JSON,
	LWS_ER_HTML
} lws_error_response_e;


#define LWS_STATCACHE_CAP_DEFAULT 1024
#define LWS_STATCACHE_TIMEOUT_DEFAULT 30


extern ngx_module_t lws;


#endif /* _LWS_MODULE_INCLUDED */
