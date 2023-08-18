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
static void lws_set_state_timer(lws_state_t *state);
static void lws_state_timer_handler(ngx_event_t *ev);


static void *lws_lua_alloc_unchecked (void *ud, void *ptr, size_t osize, size_t nsize) {
	if (nsize == 0) {
		free(ptr);
		return NULL;
	}
	return realloc(ptr, nsize);
}

static void *lws_lua_alloc_checked (void *ud, void *ptr, size_t osize, size_t nsize) {
	size_t  used_memory;

	lws_state_t *state = ud;
	if (nsize == 0) {
		free(ptr);
		state->used_memory -= osize;
		return NULL;
	}
	used_memory = state->used_memory - osize + nsize;
	if (used_memory > state->max_memory) {
		return NULL;
	}
	ptr = realloc(ptr, nsize);
	if (ptr) {
		state->used_memory = used_memory;
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

static void lws_set_state_timer (lws_state_t *state) {
	ngx_msec_t  next;

	if (state->max_time == NGX_TIMER_INFINITE && state->timeout == NGX_TIMER_INFINITE) {
		return;
	}
	if (state->tev.timer_set) {
		ngx_del_timer(&state->tev);
	}
	next = state->timeout < state->max_time ? state->timeout : state->max_time;
	ngx_add_timer(&state->tev, next - ngx_current_msec);
}

static void lws_state_timer_handler (ngx_event_t *ev) {
	lws_state_t  *state;

	state = ev->data;
	if (!state->in_use) {
		lws_close_state(state, ev->log);
	} else {
		state->close = 1;
	}
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
	if (llcf->max_memory > 0) {
		state->max_memory = llcf->max_memory;
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

	/* set timer */
	if (llcf->max_time) {
		state->max_time = ngx_current_msec + llcf->max_time;
	} else {
		state->max_time = NGX_TIMER_INFINITE;
	}
	state->timeout = NGX_TIMER_INFINITE;
	state->tev.data = state;
	state->tev.handler = lws_state_timer_handler;
	state->tev.log = ngx_cycle->log;
	lws_set_state_timer(state);

	/* done */
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
	state->in_use = 1;
	return state;
}

void lws_put_state (ngx_http_request_t *r, lws_state_t *state) {
	lws_loc_config_t  *llcf;

	llcf = ngx_http_get_module_loc_conf(r, lws);
	state->requests++;
	if (state->close || (llcf->max_requests > 0 && state->requests >= llcf->max_requests)) {
		lws_close_state(state, r->connection->log);
		return;
	}
	if (llcf->gc > 0 && state->requests % llcf->gc == 0) {
		ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
				"[LWS] pre-GC L:%p used:%d kb", state->L,
				lua_gc(state->L, LUA_GCCOUNT, 0));
		lua_gc(state->L, LUA_GCCOLLECT, 0);
		ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
				"[LWS] post-GC L:%p used:%d kb", state->L,
				lua_gc(state->L, LUA_GCCOUNT, 0));
	}
	if (llcf->timeout > 0) {
		state->timeout = ngx_current_msec + llcf->timeout;
		lws_set_state_timer(state);
	}
	state->in_use = 0;
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
		ctx->state->close = 1;
		result = -1;
	}  /* [traceback, result] */

	/* clear result */
	lua_pop(L, 1);  /* [traceback] */

	return result;
}
