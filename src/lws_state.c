/*
 * LWS state
 *
 * Copyright (C) 2023 Andre Naef
 */


#include <lws_def.h>
#include <lauxlib.h>
#include <lualib.h>
#include <lws_module.h>


static void *lws_lua_alloc_unchecked(void *ud, void *ptr, size_t osize, size_t nsize);
static void *lws_lua_alloc_checked(void *ud, void *ptr, size_t osize, size_t nsize);
static void lws_lua_set_path(lua_State *L, int index, const char *field);
static int lws_lua_init(lua_State *L);


static void *lws_lua_alloc_unchecked (void *ud, void *ptr, size_t osize, size_t nsize) {
	if (nsize == 0) {
		free(ptr);
		return NULL;
	}
	return realloc(ptr, nsize);
}

static void *lws_lua_alloc_checked (void *ud, void *ptr, size_t osize, size_t nsize) {
	size_t  memory_used;

	lws_state_t *state = ud;
	if (nsize == 0) {
		free(ptr);
		state->memory_used -= osize;
		return NULL;
	}
	memory_used = state->memory_used - osize + nsize;
	if (memory_used > state->memory_limit) {
		return NULL;
	}
	ptr = realloc(ptr, nsize);
	if (ptr) {
		state->memory_used = memory_used;
	}
	return ptr;
}

static void lws_lua_set_path (lua_State *L, int index, const char *field) {
	size_t       path_len;
	const char  *path;

	/* check path */
	path = lua_tolstring(L, index, &path_len);
	if (path_len == 0) {
		return;
	}

	/* get loader */
	if (lua_getglobal(L, LUA_LOADLIBNAME) != LUA_TTABLE) {
		luaL_error(L, "failed to get loader");
	}

	/* process path */
	if (path[0] == '+') {
		if (lua_getfield(L, -1, field) != LUA_TSTRING) {
			luaL_error(L, "failed to get loader %s", field);
		}
		lua_pushliteral(L, ";");
		lua_pushlstring(L, ++path, --path_len);
		lua_concat(L, 3);
	} else {
		lua_pushvalue(L, index);
	}
        lua_setfield(L, -2, field);
        lua_pop(L, 1);
}

static int lws_lua_init (lua_State *L) {
	/* open standard libraries */
	luaL_openlibs(L);

	/* open LWS library */
	luaL_requiref(L, LWS_LIB_NAME, lws_lua_open_lws, 1);

	/* set paths */
	lws_lua_set_path(L, 1, "path");
	lws_lua_set_path(L, 2, "cpath");

	return 0;
}

lws_state_t *lws_create_state (ngx_http_request_t *r) {
	ngx_log_t         *log;
	lws_state_t       *state;
	lws_loc_config_t  *llcf;

	/* create state */
	log = r->connection->log;
	state = ngx_calloc(sizeof(lws_state_t), log);
	if (!state) {
		ngx_log_error(NGX_LOG_CRIT, log, 0, "[LWS] failed to allocate state");
		return NULL;
	}

	/* create Lua state */
	llcf = ngx_http_get_module_loc_conf(r, lws);
	if (llcf->memory_limit > 0) {
		state->memory_limit = llcf->memory_limit;
		state->L = lua_newstate(lws_lua_alloc_checked, state);
	} else {
		state->L = lua_newstate(lws_lua_alloc_unchecked, NULL);
	}
	if (!state->L) {
		ngx_log_error(NGX_LOG_CRIT, log, 0, "[LWS] failed to create Lua state");
		return NULL;
	}

	/* initialize Lua state */
	lua_pushcfunction(state->L, lws_lua_init);
	lua_pushlstring(state->L, (const char *)llcf->path.data, llcf->path.len);
	lua_pushlstring(state->L, (const char *)llcf->cpath.data, llcf->cpath.len);
	if (lua_pcall(state->L, 2, 0, 0) != LUA_OK) {
		ngx_log_error(NGX_LOG_CRIT, log, 0, "[LWS] failed to initialize Lua state: %s",
				lws_lua_get_msg(state->L, -1));
		lws_close_state(state, log);
		return NULL;
	}

	/* push traceback */
	lua_pushcfunction(state->L, lws_lua_traceback);

	ngx_log_error(NGX_LOG_INFO, log, 0, "[LWS] %s state created L:%p", LUA_VERSION, state->L);
	return state;
}

void lws_close_state (lws_state_t *state, ngx_log_t *log) {
	lua_close(state->L);
	ngx_log_error(NGX_LOG_INFO, log, 0, "[LWS] %s state closed L:%p", LUA_VERSION, state->L);
	ngx_free(state);
}

lws_state_t *lws_get_state (ngx_http_request_t *r) {
	lws_state_t       *state;
	ngx_queue_t       *q;
	lws_loc_config_t  *llcf;

	llcf = ngx_http_get_module_loc_conf(r, lws);
	if (ngx_queue_empty(&llcf->states)) {
		state = lws_create_state(r);
	} else {
		q = ngx_queue_head(&llcf->states);
		ngx_queue_remove(q);
		state = ngx_queue_data(q, lws_state_t, queue);
	}
	return state;
}

void lws_put_state (ngx_http_request_t *r, lws_state_t *state) {
	lws_loc_config_t  *llcf;

	llcf = ngx_http_get_module_loc_conf(r, lws);
	state->lifecycles++;
	if (state->error || (llcf->lifecycles > 0 && state->lifecycles >= llcf->lifecycles)) {
		lws_close_state(state, r->connection->log);
		return;
	}
	ngx_queue_insert_head(&llcf->states, &state->queue);
}

int lws_run_state (lws_request_ctx_t *ctx) {
	int         result;
	lua_State  *L;

	/* prepare stack */
	L = ctx->state->L;
	lua_pushcfunction(L, lws_lua_run);
	lua_pushlightuserdata(L, ctx);  /* [traceback, function, ctx] */

	/* call */
	if (lua_pcall(L, 1, 1, 1) == LUA_OK) {
		result = lua_tointeger(L, -1);
	} else {
		ngx_log_error(NGX_LOG_ERR, ctx->r->connection->log, 0, "[LWS] %s error: %s",
				LUA_VERSION, lws_lua_get_msg(L, -1));
		ctx->state->error = 1;
		result = -1;
	}  /* [traceback, result] */

	/* clear result */
	lua_pop(L, 1);  /* [traceback] */

	return result;
}
