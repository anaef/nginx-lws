/*
 * LWS library
 *
 * Copyright (C) 2023 Andre Naef
 */


#include <lws_lib.h>
#include <lauxlib.h>
#include <lualib.h>
#include <lws_profiler.h>
#include <lws_http.h>


#if LUA_VERSION_NUM < 502
#define LUA_OK                              0
#define luaL_loadfilex(L, filename, mode)   luaL_loadfile(L, filename)
#define luaL_testudata(L, index, name)      lws_testudata(L, index, name)
#endif


#if LUA_VERSION_NUM < 502
typedef struct {
	FILE  *f;  /* file */
} luaL_Stream;
#endif


/* compatibility */
static inline int lws_getfield(lua_State *L, int index, const char *key);
static inline int lws_rawget(lua_State *L, int index);
#if LUA_VERSION_NUM < 502
static lua_Integer lua_tointegerx(lua_State *L, int index, int *isnum);
static void luaL_setmetatable(lua_State *L, const char *name);
#endif

/* helpers */
static void lws_strdup(lws_lua_request_ctx_t *lctx, ngx_str_t *dst, ngx_str_t *src);
static void lws_unescape_url(u_char **dst, u_char **src, size_t n);

/* request context */
static lws_lua_request_ctx_t *lws_create_lua_request_ctx(lua_State *L);
static lws_lua_request_ctx_t *lws_get_lua_request_ctx(lua_State *L);
static int lws_lua_request_ctx_tostring(lua_State *L);

/* table */
static lws_lua_table_t *lws_create_lua_table(lua_State *L);
static int lws_lua_table_index(lua_State *L);
static int lws_lua_table_newindex(lua_State *L);
static int lws_lua_table_next(lua_State *L);
#if LUA_VERSION_NUM >= 502
static int lws_lua_table_pairs(lua_State *L);
#endif
static int lws_lua_table_tostring(lua_State *L);
static int lws_lua_table_gc(lua_State *L);

/* response */
static int lws_lua_response_index(lua_State *L);
static int lws_lua_response_newindex(lua_State *L);

/* strict */
static int lws_lua_strict_index(lua_State *L);

/* file */
static luaL_Stream *lws_create_file(lua_State *L);
static int lws_close_file(lua_State *L);

/* functions */
static int lws_log(lua_State *L);
static int lws_getvariable(lua_State *L);
static int lws_redirect(lua_State *L);
static int lws_setcomplete(lua_State *L);
static int lws_setclose(lua_State *L);
static int lws_parseargs(lua_State *L);
#if LUA_VERSION_NUM < 502
static int lws_pairs(lua_State *L);
#endif

/* run */
static void lws_push_env(lws_lua_request_ctx_t *lctx);
static int lws_call(lws_lua_request_ctx_t *lctx, ngx_str_t *filename, lws_lua_chunk_e chunk);


static const char *lws_chunk_names[] = {"init", "pre", "main", "post"};
static const char *const lws_lua_log_levels[] = {
	"emerg", "alert", "crit", "err", "warn", "notice", "info", "debug", NULL
};



/*
 * compatibility
 */

static inline int lws_getfield (lua_State *L, int index, const char *key) {
#if LUA_VERSION_NUM >= 503
	return lua_getfield(L, index, key);
#else
	lua_getfield(L, index, key);
	return lua_type(L, -1);
#endif
}

static inline int lws_rawget (lua_State *L, int index) {
#if LUA_VERSION_NUM >= 503
	return lua_rawget(L, index);
#else
	lua_rawget(L, index);
	return lua_type(L, -1);
#endif
}

#if LUA_VERSION_NUM < 502
static lua_Integer lua_tointegerx (lua_State *L, int index, int *isnum) {
	if (isnum) {
		*isnum = lua_isnumber(L, index);
	}
	return lua_tointeger(L, index);
}

static void luaL_setmetatable (lua_State *L, const char *name) {
	lua_getfield(L, LUA_REGISTRYINDEX, name);
	lua_setmetatable(L, -2);
}

void *lws_testudata (lua_State *L, int index, const char *name) {
	void  *userdata;

	userdata = lua_touserdata(L, index);
	if (!userdata || !lua_getmetatable(L, index)) {
		return NULL;
	}
	luaL_getmetatable(L, name);
	if (!lua_rawequal(L, -1, -2)) {
		userdata = NULL;
	}
	lua_pop(L, 2);
	return userdata;
}
#endif


/*
 * helpers
 */

static void lws_strdup (lws_lua_request_ctx_t *lctx, ngx_str_t *dst, ngx_str_t *src) {
	dst->data = ngx_alloc(src->len, lctx->ctx->r->connection->log);
	if (!dst->data) {
		luaL_error(lctx->ctx->state->L, "failed to allocate string");
	}
	ngx_memcpy(dst->data, src->data, src->len);
	dst->len = src->len;
}

static void lws_unescape_url (u_char **dst, u_char **src, size_t n) {
	int      state;
	u_char  *d, *s, *last, c;

	d = *dst;
	s = *src;
	last = s + n;
	c = 0;
	state = 0;
	while (s < last) {
		switch (state) {
		case 0:
			switch (*s) {
			case '+':
				*d++ = ' ';
				s++;
				break;

			case '%':
				s++;
				state = 1;
				break;

			default:
				*d++ = *s++;
			}
			break;

		case 1: /* expect first hex digit */
			if (*s >= '0' && *s <= '9') {
				c = (*s++ - '0') * 16;
				state = 2;
			} else if (*s >= 'a' && *s <= 'f') {
				c = (*s++ - 'a' + 10) * 16;
				state = 2;
			} else if (*s >= 'A' && *s <= 'F') {
				c = (*s++ - 'A' + 10) * 16;
				state = 2;
			} else {
				*d++ = '%';
				state = 0;
			}
			break;

		case 2: /* expect second hex digit */
			if (*s >= '0' && *s <= '9') {
				*d++ = c + (*s++ - '0');
			} else if (*s >= 'a' && *s <= 'f') {
				*d++ = c + (*s++ - 'a' + 10);
			} else if (*s >= 'A' && *s <= 'F') {
				*d++ = c + (*s++ - 'A' + 10);
			} else {
				*d++ = '%';
				s--;
			}
			state = 0;
			break;
		}
	}
	*dst = d;
	*src = s;
}


/*
 * request context
 */

static lws_lua_request_ctx_t *lws_create_lua_request_ctx (lua_State *L) {
	lws_lua_request_ctx_t  *lctx;

	lctx = lua_newuserdata(L, sizeof(lws_lua_request_ctx_t));
	ngx_memzero(lctx, sizeof(lws_lua_request_ctx_t));
	luaL_getmetatable(L, LWS_REQUEST_CTX);
	lua_setmetatable(L, -2);
	return lctx;
}

static lws_lua_request_ctx_t *lws_get_lua_request_ctx (lua_State *L) {
	lws_lua_request_ctx_t *lctx;

	lua_getfield(L, LUA_REGISTRYINDEX, LWS_REQUEST_CTX_CURRENT);
	if (!(lctx = luaL_testudata(L, -1, LWS_REQUEST_CTX))) {
		luaL_error(L, "no request context");
	}
	lua_pop(L, 1);
	return lctx;
}

static int lws_lua_request_ctx_tostring (lua_State *L) {
	lws_lua_request_ctx_t  *lctx;

	lctx = luaL_checkudata(L, 1, LWS_REQUEST_CTX);
	lua_pushfstring(L, LWS_REQUEST_CTX ": %p", lctx->ctx);
	return 1;
}


/*
 * table
 */

static lws_lua_table_t *lws_create_lua_table (lua_State *L) {
	lws_lua_table_t  *lt;

	lt = lua_newuserdata(L, sizeof(lws_lua_table_t));
	ngx_memzero(lt, sizeof(lws_lua_table_t));
	luaL_getmetatable(L, LWS_TABLE);
	lua_setmetatable(L, -2);
	return lt;
}

static int lws_lua_table_index (lua_State *L) {
	lws_lua_table_t  *lt;
	ngx_str_t         key, *value;

	lt = luaL_checkudata(L, 1, LWS_TABLE);
	key.data = (u_char *)luaL_checklstring(L, 2, &key.len);
	value = lws_table_get(lt->t, &key);
	if (value) {
		lua_pushlstring(L, (const char *)value->data, value->len);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int lws_lua_table_newindex (lua_State *L) {
	lws_lua_table_t  *lt;
	ngx_str_t         key, value, *dup;

	lt = luaL_checkudata(L, 1, LWS_TABLE);
	if (lt->readonly) {
		return luaL_error(L, "table is read-only");
	}
	key.data = (u_char *)luaL_checklstring(L, 2, &key.len);
	value.data = (u_char *)luaL_checklstring(L, 3, &value.len);
	dup = ngx_alloc(sizeof(ngx_str_t) + value.len, lt->t->log);
	if (!dup) {
		return luaL_error(L, "failed to allocate string");
	}
	dup->data = (u_char *)dup + sizeof(ngx_str_t);
	ngx_memcpy(dup->data, value.data, value.len);
	dup->len = value.len;
	if (lws_table_set(lt->t, &key, dup) != 0) {
		ngx_free(dup);
		return luaL_error(L, "failed to set table value");
	}
	return 0;
}

static int lws_lua_table_next (lua_State *L) {
	lws_lua_table_t  *lt;
	ngx_str_t        *key, *value, prev;

	lt = luaL_checkudata(L, 1, LWS_TABLE);
	if (lua_isnoneornil(L, 2)) {
		key = NULL;
	} else {
		prev.data = (u_char *)lua_tolstring(L, 2, &prev.len);
		key = &prev;
	}
	if (lws_table_next(lt->t, key, &key, (void**)&value) != 0) {
		lua_pushnil(L);
		return 1;
	}
	lua_pushlstring(L, (const char *)key->data, key->len);
	lua_pushlstring(L, (const char *)value->data, value->len);
	return 2;
}

#if LUA_VERSION_NUM >= 502
static int lws_lua_table_pairs (lua_State *L) {
	lua_pushcfunction(L, lws_lua_table_next);
	lua_pushvalue(L, 1);
	lua_pushnil(L);
	return 3;
}
#endif

static int lws_lua_table_tostring (lua_State *L) {
	lws_lua_table_t  *lt;

	lt = luaL_checkudata(L, 1, LWS_TABLE);
	lua_pushfstring(L, LWS_TABLE ": %p", lt->t);
	return 1;
}

static int lws_lua_table_gc (lua_State *L) {
	lws_lua_table_t  *lt;

	lt = luaL_checkudata(L, 1, LWS_TABLE);
	if (!lt->external && lt->t) {
		lws_table_free(lt->t);
	}
	return 0;
}


/*
 * response
 */

static int lws_lua_response_index (lua_State *L) {
	ngx_str_t               key;
	lws_lua_request_ctx_t  *lctx;

	luaL_checktype(L, 1, LUA_TTABLE);
	key.data = (u_char *)luaL_checklstring(L, 2, &key.len);
	switch (key.len) {
	case 6:
		if (ngx_strncmp(key.data, "status", 6) == 0) {
			lctx = lws_get_lua_request_ctx(L);
			lua_pushinteger(L, lctx->ctx->status);
			return 1;
		}
		break;
	}
	lua_rawget(L, 1);
	return 1;
}

static int lws_lua_response_newindex (lua_State *L) {
	int                     status;
	ngx_str_t               key;
	lws_lua_request_ctx_t  *lctx;

	luaL_checktype(L, 1, LUA_TTABLE);
	key.data = (u_char *)luaL_checklstring(L, 2, &key.len);
	switch (key.len) {
	case 6:
		if (ngx_strncmp(key.data, "status", 6) == 0) {
			status = luaL_checkinteger(L, 3);
			lctx = lws_get_lua_request_ctx(L);
			lctx->ctx->status = status;
			return 0;
		}
		break;
	}
	lua_rawset(L, 1);
	return 0;
}


/*
 * strict
 */

static int lws_lua_strict_index (lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_rawget(L, 1);
	if (lua_isnil(L, -1)) {
		return luaL_error(L, "bad index");
	}
	return 1;
}


/*
 * file
 */

static luaL_Stream *lws_create_file (lua_State *L) {
	luaL_Stream  *s;

	s = lua_newuserdata(L, sizeof(luaL_Stream));
	s->f = NULL;
#if LUA_VERSION_NUM >= 502
	s->closef = lws_close_file;
#else
	lua_getfield(L, LUA_REGISTRYINDEX, LWS_FILE);
	lua_setfenv(L, -2);
#endif
	luaL_setmetatable(L, LUA_FILEHANDLE);
	return s;
}

static int lws_close_file (lua_State *L) {
	lua_pushboolean(L, 1);  /* "success"; the actual FILE is managed externally */
	return 1;
}


/*
 * functions
 */

static int lws_log (lua_State *L) {
	int                     index;
	ngx_str_t               msg;
	ngx_uint_t              level;
	lws_lua_request_ctx_t  *lctx;

	index = 1;
	level = lua_gettop(L) > 1 ? luaL_checkoption(L, index++, "err", lws_lua_log_levels) + 1
			: NGX_LOG_ERR;
	msg.data = (u_char *)luaL_checklstring(L, index, &msg.len);
	lctx = lws_get_lua_request_ctx(L);
	if (level != NGX_LOG_DEBUG) {
		ngx_log_error(level, lctx->ctx->r->connection->log, 0, "[LWS] %V", &msg);
	} else {
		level |= NGX_LOG_DEBUG_HTTP;
		ngx_log_debug(level, lctx->ctx->r->connection->log, 0, "[LWS] %V", &msg);
	}
	return 0;
}

static int lws_getvariable (lua_State *L) {
	ngx_str_t               key, *value;
	lws_lua_request_ctx_t  *lctx;

	key.data = (u_char *)luaL_checklstring(L, 1, &key.len);
	lctx = lws_get_lua_request_ctx(L);
	value = lws_table_get(lctx->ctx->variables, &key);
	if (value) {
		lua_pushlstring(L, (const char *)value->data, value->len);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int lws_redirect (lua_State *L) {
	ngx_str_t               redirect, args;
	lws_lua_request_ctx_t  *lctx;

	lctx = lws_get_lua_request_ctx(L);
	if (lctx->chunk != LWS_LC_PRE && lctx->chunk != LWS_LC_MAIN) {
		return luaL_error(L, "not allowed in %s chunk", lws_chunk_names[lctx->chunk]);
	}
	redirect.data = (u_char *)luaL_checklstring(L, 1, &redirect.len);
	luaL_argcheck(L, redirect.len > (redirect.data[0] != '@' ? 0 : 1), 1, "empty path or name");
	if (redirect.data[0] != '@') {
		args.data = (u_char *)luaL_optlstring(L, 2, NULL, &args.len);
		if (args.data) {
			lws_strdup(lctx, &lctx->ctx->redirect_args, &args);
		}
	}
	lws_strdup(lctx, &lctx->ctx->redirect, &redirect);
	if (lctx->chunk == LWS_LC_PRE) {
		lctx->complete = 1;
	}
	return 0;
}

static int lws_setcomplete (lua_State *L) {
	lws_lua_request_ctx_t  *lctx;

	lctx = lws_get_lua_request_ctx(L);
	if (lctx->chunk != LWS_LC_PRE) {
		return luaL_error(L, "not allowed in %s chunk", lws_chunk_names[lctx->chunk]);
	}
	lctx->complete = 1;
	return 0;
}

static int lws_setclose (lua_State *L) {
	lws_lua_request_ctx_t  *lctx;

	lctx = lws_get_lua_request_ctx(L);
	lctx->ctx->state->close = 1;
	return 0;
}

static int lws_parseargs (lua_State *L) {
	int          nrec, state;
	size_t       n;
	u_char      *start, *pos, *last, *u_start, *u_pos;
	ngx_str_t    args;
	luaL_Buffer  B;

	/* check arguments */
	args.data = (u_char *)luaL_checklstring(L, 1, &args.len);
	pos = args.data;
	last = args.data + args.len;
	if (pos == last) {
		lua_newtable(L);
		return 1;
	}

	/* create table */
	nrec = 1;
	while (pos < last) {
		if (*pos++ == '&') {
			nrec++;
		}
	}
	lua_createtable(L, 0, nrec);

	/* parse */
	state = 0;
	pos = args.data;
	while (1) {
		start = pos;
		while (pos < last && *pos != '=' && *pos != '&') {
			pos++;
		}
		n = pos - start;
		if (n > 0) {
			luaL_buffinit(L, &B);
#if LUA_VERSION_NUM >= 502
			u_start = u_pos = (u_char *)luaL_prepbuffsize(&B, n);
#else
			if (n > LUAL_BUFFERSIZE) {
				return luaL_error(L, "argument too long");
			}
			u_start = u_pos = (u_char *)luaL_prepbuffer(&B);
#endif
			lws_unescape_url(&u_pos, &start, n);
			luaL_addsize(&B, u_pos - u_start);
			luaL_pushresult(&B);
		} else {
			lua_pushliteral(L, "");
		}
		if (state == 0) {
			if (*pos == '=') {
				state = 1;
			} else {
				if (n > 0) {
					lua_pushliteral(L, "");
					lua_rawset(L, -3);
				} else {
					lua_pop(L, 1);
				}
			}
		} else {
			lua_rawset(L, -3);
			state = 0;
		}
		if (pos >= last) {
			break;
		}
		pos++;
	}
	return 1;
}

#if LUA_VERSION_NUM < 502
static int lws_pairs (lua_State *L) {
	(void)luaL_checkudata(L, 1, LWS_TABLE);
	lua_pushcfunction(L, lws_lua_table_next);
	lua_pushvalue(L, 1);
	lua_pushnil(L);
	return 3;
}
#endif

int lws_open_lws (lua_State *L) {
	int                 i;
	lws_http_status_t  *status;
	static luaL_Reg     lws_lua_functions[] = {
		{"log", lws_log},
		{"getvariable", lws_getvariable},
		{"redirect", lws_redirect},
		{"setcomplete", lws_setcomplete},
		{"setclose", lws_setclose},
		{"parseargs", lws_parseargs},
#if LUA_VERSION_NUM < 502
		{"pairs", lws_pairs},
#endif
		{NULL, NULL}
	};


	/* functions */
#if LUA_VERSION_NUM >= 502
	luaL_newlib(L, lws_lua_functions);
#else
	luaL_register(L, luaL_checkstring(L, 1), lws_lua_functions);
#endif

	/* status */
	lua_createtable(L, 0, lws_http_status_n);
	lua_createtable(L, 0, 1);
	lua_pushcfunction(L, lws_lua_strict_index);
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, -2);
	for (i = 0; i < lws_http_status_n; i++) {
		status = &lws_http_status[i];
		lua_pushlstring(L, (const char *)status->key.data, status->key.len);
		lua_pushinteger(L, status->code);
		lua_rawset(L, -3);
	}
	lua_setfield(L, -2, "status");

	/* LWS request context */
	luaL_newmetatable(L, LWS_REQUEST_CTX);
	lua_pushcfunction(L, lws_lua_request_ctx_tostring);
	lua_setfield(L, -2, "__tostring");
	lua_pop(L, 1);

	/* LWS table */
	luaL_newmetatable(L, LWS_TABLE);
	lua_pushcfunction(L, lws_lua_table_index);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, lws_lua_table_newindex);
	lua_setfield(L, -2, "__newindex");
#if LUA_VERSION_NUM >= 502
	lua_pushcfunction(L, lws_lua_table_pairs);
	lua_setfield(L, -2, "__pairs");
#endif
	lua_pushcfunction(L, lws_lua_table_tostring);
	lua_setfield(L, -2, "__tostring");
	lua_pushcfunction(L, lws_lua_table_gc);
	lua_setfield(L, -2, "__gc");
	lua_pop(L, 1);

	/* HTTP response */
	luaL_newmetatable(L, LWS_RESPONSE);
	lua_pushcfunction(L, lws_lua_response_index);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, lws_lua_response_newindex);
	lua_setfield(L, -2, "__newindex");
	lua_pop(L, 1);

#if LUA_VERSION_NUM < 502
	/* file environment */
	luaL_newmetatable(L, LWS_FILE);
	lua_pushcfunction(L, lws_close_file);
	lua_setfield(L, -2, "__close");
	lua_pop(L, 1);
#endif

	return 1;
}


/*
 * trace
 */

void lws_get_msg (lua_State *L, int index, ngx_str_t *msg) {
	if (!lua_isnone(L, index)) {
		if (lua_isstring(L, index)) {
			msg->data = (u_char *)lua_tolstring(L, index, &msg->len);
		} else {
			ngx_str_set(msg, "(error value is not a string)");
		}
	} else {
		ngx_str_set(msg, "(no error value)");
	}
}

int lws_traceback (lua_State *L) {
	ngx_str_t  msg;

	lws_get_msg(L, 1, &msg);
#if LUA_VERSION_NUM >= 502
	luaL_traceback(L, L, (const char *)msg.data, 1);
#else
	if (lws_getfield(L, LUA_GLOBALSINDEX, LUA_DBLIBNAME) == LUA_TTABLE
			&& lws_getfield(L, -1, "traceback") == LUA_TFUNCTION) {
		lua_pushlstring(L, (char *)msg.data, msg.len);
		lua_pushinteger(L, 2);
		lua_call(L, 2, 1);
	} else {
		lua_pushlstring(L, (char *)msg.data, msg.len);
	}
#endif
	return 1;
}


/*
 * run
 */

static void lws_push_env (lws_lua_request_ctx_t *lctx) {
	lua_State           *L;
	luaL_Stream         *request_body, *response_body;
	lws_lua_table_t     *lt;
	lws_request_ctx_t   *ctx;
	ngx_connection_t    *c;
	ngx_http_request_t  *r;

	/* create environment */
	ctx = lctx->ctx;
	L = ctx->state->L;
	lua_newtable(L);
	lua_createtable(L, 0, 1);
#if LUA_VERSION_NUM >= 502
	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
#else
	lua_pushvalue(L, LUA_GLOBALSINDEX);
#endif
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, -2);

	/* request */
	r = ctx->r;
	lua_createtable(L, 0, 8);
	lua_pushlstring(L, (const char *)r->method_name.data, r->method_name.len);
	lua_setfield(L, -2, "method");
	lua_pushlstring(L, (const char *)r->unparsed_uri.data, r->unparsed_uri.len);
	lua_setfield(L, -2, "uri");
	lua_pushlstring(L, (const char *)r->uri.data, r->uri.len);
	lua_setfield(L, -2, "path");
	lua_pushlstring(L, (const char *)r->args.data, r->args.len);
	lua_setfield(L, -2, "args");
	lt = lws_create_lua_table(L);
	lt->t = ctx->request_headers;
	lt->readonly = 1;  /* required as key dup and free are not enabled */
	lt->external = 1;  /* will be freed externally */
	lua_setfield(L, -2, "headers");
	request_body = lws_create_file(L);
	request_body->f = ctx->request_body;
	lua_setfield(L, -2, "body");
	lua_pushlstring(L, (const char *)ctx->path_info.data, ctx->path_info.len);
	lua_setfield(L, -2, "path_info");
	c = r->connection;
	if ((c->sockaddr->sa_family == AF_INET || c->sockaddr->sa_family == AF_INET6)
			&& c->addr_text.len) {
		lua_pushlstring(L, (const char *)c->addr_text.data, c->addr_text.len);
		lua_setfield(L, -2, "ip");
	}
	lua_setfield(L, -2, "request");

	/* response */
	lua_createtable(L, 0, 2);
	luaL_getmetatable(L, LWS_RESPONSE);
	lua_setmetatable(L, -2);
	lt = lws_create_lua_table(L);
	lt->t = ctx->response_headers;
	lt->external = 1;  /* see request above */
	lua_setfield(L, -2, "headers");
	response_body = lws_create_file(L);
	response_body->f = ctx->response_body;
	lua_setfield(L, -2, "body");
	lua_setfield(L, -2, "response");
}

static int lws_call (lws_lua_request_ctx_t *lctx, ngx_str_t *filename, lws_lua_chunk_e chunk) {
	int         result, isint;
	lua_State  *L;

	/* set chunk */
	lctx->chunk = chunk;

	/* get, or load and store, the function */
	L = lctx->ctx->state->L;
	lua_pushlstring(L, (const char *)filename->data, filename->len);  /* [filename] */
	lua_pushvalue(L, -1);  /* [filename, filename] */
	if (lws_rawget(L, 2) != LUA_TFUNCTION) {  /* [filename, x] */
		lua_pop(L, 1);  /* [filename] */
		if (luaL_loadfilex(L, lua_tostring(L, -1), "bt") != LUA_OK) {
			return lua_error(L);
		}  /* [filename, function] */
		lua_pushvalue(L, -2);  /* [filename, function, filename] */
		lua_pushvalue(L, -2);  /* [filename, function, filename, function] */
		lua_rawset(L, 2);      /* [filename, function] */
	}  /* [filename, function] */

	/* set _ENV */
	if (chunk != LWS_LC_INIT) {
		lua_pushvalue(L, 3);
	} else {
#if LUA_VERSION_NUM >= 502
		lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
#else
		lua_pushvalue(L, LUA_GLOBALSINDEX);
#endif
	}  /* [filename, function, env] */
#if LUA_VERSION_NUM >= 502
	lua_setupvalue(L, -2, 1);  /* _ENV is the first upvalue */
#else
	lua_setfenv(L, -2);
#endif  /* [filename, function] */

	/* call the function */
	ngx_log_debug2(NGX_LOG_DEBUG_HTTP, lctx->ctx->r->connection->log, 0,
			"[LWS] calling %s chunk filename:%V", lws_chunk_names[chunk],
			filename);
	lua_call(L, 0, 1);  /* [filename, result] */

	/* check result */
	if (lua_isnil(L, -1)) {
		result = 0;
	} else {
		result = lua_tointegerx(L, -1, &isint);
		if (!isint) {
			ngx_log_error(NGX_LOG_WARN, lctx->ctx->r->connection->log, 0,
					"[LWS] bad result type (nil or integer expected, got %s)",
					luaL_typename(L, -1));
			result = -1;
		}
	}
	if (result < 0) {
		return luaL_error(L, "%s: %s chunk failed (%d)", lua_tostring(L, -2),
				lws_chunk_names[chunk], result);
	}
	if (result > 0 && chunk == LWS_LC_PRE) {
		lctx->complete = 1;
	}

	/* finish */
	lua_pop(L, 2);  /* [] */
	return result;
}

int lws_run (lua_State *L) {
	int                     result;
	lws_request_ctx_t      *ctx;
	lws_lua_request_ctx_t  *lctx;

	/* get arguments */
	ctx = (void *)lua_topointer(L, 1);  /* [ctx] */

	/* set request context */
	lctx = lws_create_lua_request_ctx(L);
	lctx->ctx = ctx;
	lua_setfield(L, LUA_REGISTRYINDEX, LWS_REQUEST_CTX_CURRENT);  /* [ctx] */

	/* get chunks */
	if (lws_getfield(L, LUA_REGISTRYINDEX, LWS_CHUNKS) != LUA_TTABLE) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, LWS_CHUNKS);
	}  /* [ctx, chunks] */

	/* start profiler */
	if (ctx->state->profiler) {
		lua_pushcfunction(L, lws_start_profiler);
		lua_pushlightuserdata(L, ctx);
		lua_call(L, 1, 0);
	}

	/* init */
	if (!ctx->state->init) {
		if (ctx->state->llcf->init.len) {
			(void)lws_call(lctx, &ctx->state->llcf->init, LWS_LC_INIT);
		}
		ctx->state->init = 1;
	}

	/* push environment */
	lws_push_env(lctx);  /* [ctx, chunks, env] */

	/* pre chunk */
	if (ctx->state->llcf->pre.len) {
		result = lws_call(lctx, &ctx->state->llcf->pre, LWS_LC_PRE);
		if (lctx->complete) {
			goto post;
		}  /* result is invariably 0 at this point */
	}

	/* main chunk */
	result = lws_call(lctx, &ctx->main, LWS_LC_MAIN);

	/* post chunk */
	post:
	if (ctx->state->llcf->post.len) {
		(void)lws_call(lctx, &ctx->state->llcf->post, LWS_LC_POST);
	}

	/* stop profiler */
	if (ctx->state->profiler) {
		lua_pushcfunction(L, lws_stop_profiler);
		lua_call(L, 0, 0);
	}

	/* clear request context */
	lua_pushnil(L);
	lua_setfield(L, LUA_REGISTRYINDEX, LWS_REQUEST_CTX_CURRENT);  /* [ctx, chunks, env] */

	/* return result */
	lua_pushinteger(L, result);  /* [ctx, chunks, env, result] */
	return 1;
}
