/*
 * LWS library
 *
 * Copyright (C) 2023 Andre Naef
 */


#include <lws_def.h>
#include <lauxlib.h>
#include <lualib.h>
#include <lws_module.h>


static ngx_str_t *lws_lua_strdup(lua_State *L, ngx_str_t *str, ngx_log_t *log);
static void lws_lua_unescape_url(u_char **dest, u_char **src, size_t n);

static lws_lua_request_ctx_t *lws_lua_create_request_ctx(lua_State *L);
static int lws_lua_request_ctx_tostring(lua_State *L);
static lws_lua_request_ctx_t *lws_lua_get_request_ctx(lua_State *L);

static lws_lua_table_t *lws_lua_create_table(lua_State *L);
static int lws_lua_table_index(lua_State *L);
static int lws_lua_table_newindex(lua_State *L);
static int lws_lua_table_next(lua_State *L);
static int lws_lua_table_pairs(lua_State *L);
static int lws_lua_table_tostring(lua_State *L);
static int lws_lua_table_gc(lua_State *L);

static int lws_lua_response_index(lua_State *L);
static int lws_lua_response_newindex(lua_State *L);

static int lws_lua_strict_index(lua_State *L);

static luaL_Stream *lws_lua_create_file(lua_State *L);
static int lws_lua_close_file(lua_State *L);

static int lws_lua_log(lua_State *L);
static int lws_lua_redirect(lua_State *L);
static int lws_lua_setcomplete(lua_State *L);
static int lws_lua_parseargs(lua_State *L);

static void lws_lua_push_env(lws_request_ctx_t *ctx);
static int lws_lua_call(lws_request_ctx_t *ctx, ngx_str_t *filename, const char *chunk, int env);


/*
 * helpers
 */

static ngx_str_t *lws_lua_strdup (lua_State *L, ngx_str_t *str, ngx_log_t *log) {
	ngx_str_t  *dup;

	dup = ngx_alloc(sizeof(ngx_str_t) + str->len, log);
	if (!dup) {
		luaL_error(L, "failed to allocate string");
	}
	dup->data = (u_char *)dup + sizeof(ngx_str_t);
	ngx_memcpy(dup->data, str->data, str->len);
	dup->len = str->len;
	return dup;
}

static void lws_lua_unescape_url (u_char **dest, u_char **src, size_t n) {
	int      state;
	u_char  *d, *s, *end, c;

	d = *dest;
	s = *src;
	end = s + n;
	c = 0;
	state = 0;
	while (s < end) {
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
	*dest = d;
	*src = s;
}


/*
 * request context
 */

static lws_lua_request_ctx_t *lws_lua_create_request_ctx (lua_State *L) {
	lws_lua_request_ctx_t  *lctx;

	lctx = lua_newuserdata(L, sizeof(lws_lua_request_ctx_t));
	lctx->ctx = NULL;
	luaL_getmetatable(L, LWS_LIB_REQUEST_CTX);
	lua_setmetatable(L, -2);
	return lctx;
}

static int lws_lua_request_ctx_tostring (lua_State *L) {
	lws_lua_request_ctx_t  *lctx;

	lctx = luaL_checkudata(L, 1, LWS_LIB_REQUEST_CTX);
	lua_pushfstring(L, LWS_LIB_REQUEST_CTX ": %p", lctx->ctx);
	return 1;
}

static lws_lua_request_ctx_t *lws_lua_get_request_ctx (lua_State *L) {
	lws_lua_request_ctx_t *lctx;

	lua_getfield(L, LUA_REGISTRYINDEX, LWS_LIB_REQUEST_CTX_CURRENT);
	if (!(lctx = luaL_testudata(L, -1, LWS_LIB_REQUEST_CTX))) {
		luaL_error(L, "no request context");
	}
	lua_pop(L, 1);
	return lctx;
}


/*
 * table
 */

static lws_lua_table_t *lws_lua_create_table (lua_State *L) {
	lws_lua_table_t  *lt;

	lt = lua_newuserdata(L, sizeof(lws_lua_table_t));
	ngx_memzero(lt, sizeof(lws_lua_table_t));
	luaL_getmetatable(L, LWS_LIB_TABLE);
	lua_setmetatable(L, -2);
	return lt;
}

static int lws_lua_table_index (lua_State *L) {
	lws_lua_table_t  *lt;
	ngx_str_t         key, *value;

	lt = luaL_checkudata(L, 1, LWS_LIB_TABLE);
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

	lt = luaL_checkudata(L, 1, LWS_LIB_TABLE);
	if (lt->readonly) {
		return luaL_error(L, "table is read-only");
	}
	key.data = (u_char *)luaL_checklstring(L, 2, &key.len);
	value.data = (u_char *)luaL_checklstring(L, 3, &value.len);
	dup = lws_lua_strdup(L, &value, lt->t->log);
	if (lws_table_set(lt->t, &key, dup) != 0) {
		ngx_free(dup);
		return luaL_error(L, "failed to set table value");
	}
	return 0;
}

static int lws_lua_table_next (lua_State *L) {
	lws_lua_table_t  *lt;
	ngx_str_t        *key, *value, prev;

	lt = luaL_checkudata(L, 1, LWS_LIB_TABLE);
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

static int lws_lua_table_pairs (lua_State *L) {
	lua_pushcfunction(L, lws_lua_table_next);
	lua_pushvalue(L, 1);
	lua_pushnil(L);
	return 3;
}

static int lws_lua_table_tostring (lua_State *L) {
	lws_lua_table_t  *lt;

	lt = luaL_checkudata(L, 1, LWS_LIB_TABLE);
	lua_pushfstring(L, LWS_LIB_TABLE ": %p", lt->t);
	return 1;
}

static int lws_lua_table_gc (lua_State *L) {
	lws_lua_table_t  *lt;

	lt = luaL_checkudata(L, 1, LWS_LIB_TABLE);
	if (!lt->external && lt->t) {
		lws_table_free(lt->t);
	}
	return 0;
}


/*
 * response
 */

static int lws_lua_response_index (lua_State *L) {
	size_t                  key_len;
	const char             *key;
	lws_lua_request_ctx_t  *lctx;

	luaL_checktype(L, 1, LUA_TTABLE);
	key = luaL_checklstring(L, 2, &key_len);
	switch (key_len) {
	case 6:
		if (strncmp(key, "status", 6) == 0) {
			lctx = lws_lua_get_request_ctx(L);
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
	size_t                  key_len;
	const char             *key;
	lws_lua_request_ctx_t  *lctx;

	luaL_checktype(L, 1, LUA_TTABLE);
	key = luaL_checklstring(L, 2, &key_len);
	switch (key_len) {
	case 6:
		if (strncmp(key, "status", 6) == 0) {
			status = luaL_checkinteger(L, 3);
			lctx = lws_lua_get_request_ctx(L);
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
 * stream
 */

static luaL_Stream *lws_lua_create_file (lua_State *L) {
	luaL_Stream  *s;

	s = lua_newuserdata(L, sizeof(luaL_Stream));
	s->f = NULL;
	s->closef = lws_lua_close_file;
	luaL_setmetatable(L, LUA_FILEHANDLE);
	return s;
}

static int lws_lua_close_file (lua_State *L) {
	lua_pushboolean(L, 1); /* "success"; the actual FILE is managed externally */
	return 1;
}


/*
 * lib
 */

static const char *const lws_lua_log_levels[] = {
	"emerg", "alert", "crit", "err", "warn", "notice", "info", "debug", NULL
};

static int lws_lua_log (lua_State *L) {
	int                     index;
	ngx_str_t               msg;
	ngx_uint_t              level;
	lws_lua_request_ctx_t  *lctx;

	index = 1;
	level = lua_gettop(L) > 1 ? luaL_checkoption(L, index++, "err", lws_lua_log_levels) + 1
			: NGX_LOG_ERR;
	msg.data = (u_char *)luaL_checklstring(L, index, &msg.len);
	lctx = lws_lua_get_request_ctx(L);
	if (level != NGX_LOG_DEBUG) {
		ngx_log_error(level, lctx->ctx->r->connection->log, 0, "[LWS] %V", &msg);
	} else {
		level |= NGX_LOG_DEBUG_HTTP;
		ngx_log_debug(level, lctx->ctx->r->connection->log, 0, "[LWS] %V", &msg);
	}
	return 0;
}

static int lws_lua_redirect (lua_State *L) {
	ngx_log_t              *log;
	ngx_str_t               redirect, args;
	lws_lua_request_ctx_t  *lctx;

	redirect.data = (u_char *)luaL_checklstring(L, 1, &redirect.len);
	luaL_argcheck(L, redirect.len > (redirect.data[0] == '@' ? 1 : 0), 1, "empty path or name");
	lctx = lws_lua_get_request_ctx(L);
	log = lctx->ctx->r->connection->log;
	if (redirect.data[0] != '@') {
		args.data = (u_char *)luaL_optlstring(L, 2, NULL, &args.len);
		if (args.data) {
			lctx->ctx->redirect_args = lws_lua_strdup(L, &args, log);
		}
	}
	lctx->ctx->redirect = lws_lua_strdup(L, &redirect, log);
	lctx->ctx->complete = 1;
	return 0;
}

static int lws_lua_setcomplete (lua_State *L) {
	lws_lua_request_ctx_t  *lctx;

	lctx = lws_lua_get_request_ctx(L);
	lctx->ctx->complete = 1;
	return 0;
}

static int lws_lua_parseargs (lua_State *L) {
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
			u_start = u_pos = (u_char *)luaL_prepbuffsize(&B, n);
			lws_lua_unescape_url(&u_pos, &start, n);
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

static luaL_Reg lws_lua_functions[] = {
	{ "log", lws_lua_log },
	{ "redirect", lws_lua_redirect },
	{ "setcomplete", lws_lua_setcomplete },
	{ "parseargs", lws_lua_parseargs },
	{ NULL, NULL }
};

typedef struct lws_lua_http_status_s lws_lua_http_status_t;

struct lws_lua_http_status_s {
	ngx_int_t    code;
	const char  *key;
};

static lws_lua_http_status_t lws_lua_http_status[] = {
	/* 1xx [n=3] */
	{ NGX_HTTP_CONTINUE, "CONTINUE" },
	{ NGX_HTTP_SWITCHING_PROTOCOLS, "SWITCHING_PROTOCOLS" },
	{ NGX_HTTP_PROCESSING, "PROCESSING" },

	/* 2xx [n=7] */
	{ NGX_HTTP_OK, "OK" },
	{ NGX_HTTP_CREATED, "CREATED" },
	{ NGX_HTTP_ACCEPTED, "ACCEPTED" },
	{ 203, "NON_AUTHORITATIVE_INFORMATION" },
	{ NGX_HTTP_NO_CONTENT, "NO_CONTENT" },
	{ 205, "RESET_CONTENT" },
	{ NGX_HTTP_PARTIAL_CONTENT, "PARTIAL_CONTENT" },

	/* 3xx [n=8] */
	{ NGX_HTTP_SPECIAL_RESPONSE, "MULTIPLE_CHOICES" },
	{ NGX_HTTP_MOVED_PERMANENTLY, "MOVED_PERMANENTLY" },
	{ NGX_HTTP_MOVED_TEMPORARILY, "FOUND" },
	{ NGX_HTTP_SEE_OTHER, "SEE_OTHER" },
	{ NGX_HTTP_NOT_MODIFIED, "NOT_MODIFIED" },
	{ 305, "USE_PROXY" },
	{ NGX_HTTP_TEMPORARY_REDIRECT, "TEMPORARY_REDIRECT" },
	{ NGX_HTTP_PERMANENT_REDIRECT, "PERMANENT_REDIRECT" },

	/* 4xx [n=20] */
	{ NGX_HTTP_BAD_REQUEST, "BAD_REQUEST" },
	{ NGX_HTTP_UNAUTHORIZED, "UNAUTHORIZED" },
	{ 402, "PAYMENT_REQUIRED" },
	{ NGX_HTTP_FORBIDDEN, "FORBIDDEN" },
	{ NGX_HTTP_NOT_FOUND, "NOT_FOUND" },
	{ NGX_HTTP_NOT_ALLOWED, "METHOD_NOT_ALLOWED" },
	{ 406, "NOT_ACCEPTABLE" },
	{ 407, "PROXY_AUTHENTICATION_REQUIRED" },
	{ NGX_HTTP_REQUEST_TIME_OUT, "REQUEST_TIMEOUT" },
	{ NGX_HTTP_CONFLICT, "CONFLICT" },
	{ 410, "GONE" },
	{ NGX_HTTP_LENGTH_REQUIRED, "LENGTH_REQUIRED" },
	{ NGX_HTTP_PRECONDITION_FAILED, "PRECONDITION_FAILED" },
	{ NGX_HTTP_REQUEST_ENTITY_TOO_LARGE, "REQUEST_ENTITY_TOO_LARGE" },
	{ NGX_HTTP_REQUEST_URI_TOO_LARGE, "REQUEST_URI_TOO_LARGE" },
	{ NGX_HTTP_UNSUPPORTED_MEDIA_TYPE, "UNSUPPORTED_MEDIA_TYPE" },
	{ NGX_HTTP_RANGE_NOT_SATISFIABLE, "REQUEST_RANGE_NOT_SATISFIABLE" },
	{ 417, "EXPECTATION_FAILED" },
	{ NGX_HTTP_MISDIRECTED_REQUEST, "MISDIRECTED_REQUEST" },
	{ NGX_HTTP_TOO_MANY_REQUESTS, "TOO_MANY_REQUESTS" },

	/* 5xx [n=7] */
	{ NGX_HTTP_INTERNAL_SERVER_ERROR, "INTERNAL_SERVER_ERROR" },
	{ NGX_HTTP_NOT_IMPLEMENTED, "NOT_IMPLEMENTED" },
	{ NGX_HTTP_BAD_GATEWAY, "BAD_GATEWAY" },
	{ NGX_HTTP_SERVICE_UNAVAILABLE, "SERVICE_UNAVAILABLE" },
	{ NGX_HTTP_GATEWAY_TIME_OUT, "GATEWAY_TIMEOUT" },
	{ NGX_HTTP_VERSION_NOT_SUPPORTED, "HTTP_VERSION_NOT_SUPPORTED" },
	{ NGX_HTTP_INSUFFICIENT_STORAGE, "INSUFFICIENT_STORAGE" },
	{ 0, NULL }
};

int lws_lua_open_lws (lua_State *L) {
	lws_lua_http_status_t  *status;

	/* functions */
	luaL_newlib(L, lws_lua_functions);

	/* status */
	lua_createtable(L, 0, 3 + 7 + 8 + 20 + 7);  /* sum(n)  */
	lua_createtable(L, 0, 1);
	lua_pushcfunction(L, lws_lua_strict_index);
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, -2);
	status = lws_lua_http_status;
	while (status->code) {
		lua_pushinteger(L, status->code);
		lua_setfield(L, -2, status->key);
		status++;
	}
	lua_setfield(L, -2, "status");

	/* LWS request context */
	luaL_newmetatable(L, LWS_LIB_REQUEST_CTX);
	lua_pushcfunction(L, lws_lua_request_ctx_tostring);
	lua_setfield(L, -2, "__tostring");
	lua_pop(L, 1);

	/* LWS table */
	luaL_newmetatable(L, LWS_LIB_TABLE);
	lua_pushcfunction(L, lws_lua_table_index);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, lws_lua_table_newindex);
	lua_setfield(L, -2, "__newindex");
	lua_pushcfunction(L, lws_lua_table_pairs);
	lua_setfield(L, -2, "__pairs");
	lua_pushcfunction(L, lws_lua_table_tostring);
	lua_setfield(L, -2, "__tostring");
	lua_pushcfunction(L, lws_lua_table_gc);
	lua_setfield(L, -2, "__gc");
	lua_pop(L, 1);

	/* HTTP response */
	luaL_newmetatable(L, LWS_LIB_RESPONSE);
	lua_pushcfunction(L, lws_lua_response_index);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, lws_lua_response_newindex);
	lua_setfield(L, -2, "__newindex");
	lua_pop(L, 1);

	return 1;
}


/*
 * trace
 */

void lws_lua_get_msg (lua_State *L, int index, ngx_str_t *msg) {
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

int lws_lua_traceback (lua_State *L) {
	ngx_str_t  msg;

	lws_lua_get_msg(L, 1, &msg);
	luaL_traceback(L, L, (const char *)msg.data, 1);
	return 1;
}


/*
 * run
 */

static void lws_lua_push_env (lws_request_ctx_t *ctx) {
	lua_State           *L;
	luaL_Stream         *request_body, *response_body;
	lws_lua_table_t     *lt;
	ngx_connection_t    *c;
	ngx_http_request_t  *r;

	/* create environment */
	L = ctx->state->L;
	lua_newtable(L);
	lua_createtable(L, 0, 1);
	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
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
	lt = lws_lua_create_table(L);
	lt->t = ctx->request_headers;
	lt->readonly = 1;  /* required as key dup and free are not enabled */
	lt->external = 1;  /* will be freed externally */
	lua_setfield(L, -2, "headers");
	request_body = lws_lua_create_file(L);
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
	luaL_getmetatable(L, LWS_LIB_RESPONSE);
	lua_setmetatable(L, -2);
	lt = lws_lua_create_table(L);
	lt->t = ctx->response_headers;
	lt->external = 1;  /* see request above */
	lua_setfield(L, -2, "headers");
	response_body = lws_lua_create_file(L);
	response_body->f = ctx->response_body;
	lua_setfield(L, -2, "body");
	lua_setfield(L, -2, "response");
}

static int lws_lua_call (lws_request_ctx_t *ctx, ngx_str_t *filename, const char *chunk, int env) {
	int         result, isint;
	lua_State  *L;

	/* get, or load and store, the function */
	L = ctx->state->L;
	lua_pushlstring(L, (const char *)filename->data, filename->len);  /* [filename] */
	lua_pushvalue(L, -1);  /* [filename, filename] */
	if (lua_rawget(L, 2) != LUA_TFUNCTION) {  /* [filename, x] */
		lua_pop(L, 1);  /* [filename] */
		if (luaL_loadfilex(L, lua_tostring(L, -1), "bt") != 0) {
			return lua_error(L);
		}  /* [filename, function] */
		lua_pushvalue(L, -2);  /* [filename, function, filename] */
		lua_pushvalue(L, -2);  /* [filename, function, filename, function] */
		lua_rawset(L, 2);      /* [filename, function] */
	}  /* [filename, function] */

	/* set _ENV */
	if (env) {
		lua_pushvalue(L, 3);
	} else {
		lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
	}  /* [filename, function, env] */
	lua_setupvalue(L, -2, 1);  /* _ENV is the first upvalue; [filename, function] */

	/* call the function */
	lua_call(L, 0, 1);  /* [filename, result] */

	/* check result */
	if (lua_isnil(L, -1)) {
		result = 0;
	} else {
		result = lua_tointegerx(L, -1, &isint);
		if (!isint) {
			ngx_log_error(NGX_LOG_WARN, ctx->r->connection->log, 0,
					"[LWS] bad result type (integer or nil expected, got %s)",
					luaL_typename(L, -1));
			result = -1;
		}
	}
	if (result < 0) {
		luaL_error(L, "%s: %s chunk failed (%d)", lua_tostring(L, -2), chunk, result);
	}
	if (env && result > 0) {
		ctx->complete = 1;
	}

	/* finish */
	lua_pop(L, 2);  /* [] */
	return result;
}

int lws_lua_run (lua_State *L) {
	int                     result, post_result;
	lws_request_ctx_t      *ctx;
	lws_lua_request_ctx_t  *lctx;

	/* get arguments */
	ctx = (void *)lua_topointer(L, 1);  /* [ctx] */

	/* set request context */
	lctx = lws_lua_create_request_ctx(L);
	lctx->ctx = ctx;
	lua_setfield(L, LUA_REGISTRYINDEX, LWS_LIB_REQUEST_CTX_CURRENT);  /* [ctx] */

	/* get chunks */
	if (lua_getfield(L, LUA_REGISTRYINDEX, LWS_LIB_CHUNKS) != LUA_TTABLE) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, LWS_LIB_CHUNKS);
	} /* [ctx, chunks] */

	/* init */
	if (!ctx->state->init) {
		if (ctx->llcf->init.len) {
			(void)lws_lua_call(ctx, &ctx->llcf->init, "init", 0);
		}
		ctx->state->init = 1;
	}

	/* push environment */
	lws_lua_push_env(ctx);  /* [ctx, chunks, env] */

	/* pre-handler */
	if (ctx->llcf->pre.len) {
		result = lws_lua_call(ctx, &ctx->llcf->pre, "pre", 1);
		if (ctx->complete) {
			goto post;
		}  /* result is invariably 0 at this point */
	}

	/* main handler */
	result = lws_lua_call(ctx, &ctx->main, "main", 1);

	/* post-handler */
	post:
	if (ctx->llcf->post.len) {
		if ((post_result = lws_lua_call(ctx, &ctx->llcf->post, "post", 1)) > 0) {
			result = post_result;
		}
	}

	/* clear request context */
	lua_pushnil(L);
	lua_setfield(L, LUA_REGISTRYINDEX, LWS_LIB_REQUEST_CTX_CURRENT);  /* [ctx, chunks, env] */

	/* return result */
	lua_pushinteger(L, result);  /* [ctx, chunk, env, result] */
	return 1;
}
