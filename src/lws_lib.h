/*
 * LWS library
 *
 * Copyright (C) 2023 Andre Naef
 */


#ifndef _LWS_LIBRARY_INCLUDED
#define _LWS_LIBRARY_INCLUDED


#include <lua.h>


#define LWS_LIB_NAME "lws"

#define LWS_LIB_REQUEST_CTX "lws.request_ctx"                  /* request context metatable */
#define LWS_LIB_REQUEST_CTX_CURRENT "lws.request_ctx_current"  /* current request context */
#define LWS_LIB_TABLE "lws.table"                              /* table metatable */
#define LWS_LIB_RESPONSE "lws.response"                        /* response metatable */
#define LWS_LIB_CHUNKS "lws.chunks"                            /* loaded chunks */


typedef struct lws_lua_request_ctx_s lws_lua_request_ctx_t;
struct lws_lua_request_ctx_s {
	lws_request_ctx_t  *ctx;
};

typedef struct lws_lua_table_s lws_lua_table_t;
struct lws_lua_table_s {
	lws_table_t  *t;
	unsigned      readonly:1;
	unsigned      external:1;
};


const char *lws_lua_get_msg(lua_State *L, int index);
int lws_lua_traceback(lua_State *L);
int lws_lua_open_lws(lua_State *L);
int lws_lua_run(lua_State *L);


#endif /* _LWS_LIBRARY_INCLUDED */
