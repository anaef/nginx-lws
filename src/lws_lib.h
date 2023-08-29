/*
 * LWS library
 *
 * Copyright (C) 2023 Andre Naef
 */


#ifndef _LWS_LIBRARY_INCLUDED
#define _LWS_LIBRARY_INCLUDED


#include <lua.h>
#include <lws_module.h>


typedef struct lws_lua_request_ctx_s lws_lua_request_ctx_t;
typedef struct lws_lua_table_s lws_lua_table_t;

typedef enum {
	LWS_LC_INIT,
	LWS_LC_PRE,
	LWS_LC_MAIN,
	LWS_LC_POST
} lws_lua_chunk_e;

struct lws_lua_request_ctx_s {
	lws_request_ctx_t  *ctx;         /* request context */
	lws_lua_chunk_e     chunk;       /* current chunk */
	unsigned            complete:1;  /* request is complete */
};

struct lws_lua_table_s {
	lws_table_t  *t;           /* table */
	unsigned      readonly:1;  /* read-only access */
	unsigned      external:1;  /* managed externally */
};


void lws_lua_get_msg(lua_State *L, int index, ngx_str_t *msg);
int lws_lua_traceback(lua_State *L);
int lws_lua_open_lws(lua_State *L);
int lws_lua_run(lua_State *L);


#define LWS_LIB_NAME                 "lws"                      /* library name */
#define LWS_LIB_REQUEST_CTX          "lws.request_ctx"          /* request context metatable */
#define LWS_LIB_REQUEST_CTX_CURRENT  "lws.request_ctx_current"  /* current request context */
#define LWS_LIB_TABLE                "lws.table"                /* table metatable */
#define LWS_LIB_RESPONSE             "lws.response"             /* response metatable */
#define LWS_LIB_CHUNKS               "lws.chunks"               /* loaded chunks */


#endif /* _LWS_LIBRARY_INCLUDED */
