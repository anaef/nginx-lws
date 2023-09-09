/*
 * LWS profiler
 *
 * Copyright (C) 2023 Andre Naef
 */


#include <lws_profiler.h>
#include <lauxlib.h>
#include <lws_module.h>


static int lws_profiler_tostring(lua_State *L);
static int lws_profiler_gc(lua_State *L);
static inline void lws_add_timespec(struct timespec *base, const struct timespec *inc);
static inline void lws_add_timespec_delta(struct timespec *base, const struct timespec *from,
		const struct timespec *to);
static inline void lws_add_memory_delta(size_t *base, size_t from, size_t to);
static void lws_profiler_hook(lua_State *L, lua_Debug *ar);


static const char *const lws_profiler_state_names[] = {"disabled", "CPU", "wall"};


static int lws_profiler_tostring (lua_State *L) {
	lws_profiler_t  *p;

	p = luaL_checkudata(L, 1, LWS_PROFILER);
	lua_pushfstring(L, LWS_PROFILER ": %p", p);
	return 1;
}

static int lws_profiler_gc (lua_State *L) {
	lws_profiler_t  *p;

	p = luaL_checkudata(L, 1, LWS_PROFILER);
	if (p->functions) {
		lws_table_free(p->functions);
	}
	ngx_free(p->stack);
	return 0;
}

static inline void lws_add_timespec (struct timespec *base, const struct timespec *inc) {
	base->tv_nsec += inc->tv_nsec;
	base->tv_sec += inc->tv_sec;
	if (base->tv_nsec > 1000000000) {
		base->tv_nsec -= 1000000000;
		base->tv_sec++;
	}
}

static inline void lws_add_timespec_delta (struct timespec *base, const struct timespec *from,
		const struct timespec *to) {
	base->tv_nsec += to->tv_nsec - from->tv_nsec;
	base->tv_sec += to->tv_sec - from->tv_sec;
	if (base->tv_nsec < 0) {
		base->tv_nsec += 1000000000;
		base->tv_sec--;
	} else if (base->tv_nsec > 1000000000) {
		base->tv_nsec -= 1000000000;
		base->tv_sec++;
	}
}

static inline void lws_add_memory_delta (size_t *base, size_t from, size_t to) {
	if (to > from) {
		*base += to - from;
	}
}

static void lws_profiler_hook (lua_State *L, lua_Debug *ar) {
	u_char                    buf[LWS_PROFILER_KEY_MAX];
	u_char                   *name, *last;
	size_t                    memory, stack_alloc_new;
	ngx_str_t                 key;
	lws_profiler_t           *p;
	struct timespec           time;
	lws_activation_record_t  *par, **stack_new;

	/* get profiler */
	lua_getfield(L, LUA_REGISTRYINDEX, LWS_PROFILER_CURRENT);
	if (!(p = luaL_testudata(L, -1, LWS_PROFILER))) {
		luaL_error(L, "failed to get profiler");
	}
	lua_pop(L, 1);

	/* get time and memory on hook entry */
	if (clock_gettime(p->clock, &time) != 0) {
		luaL_error(L, "failed to get profiler %s time", lws_profiler_state_names[p->state]);
	}
	memory = (size_t)lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0);

	/* process function exit */
	if (p->stack_n > 0) {
		par = p->stack[p->stack_n - 1];
		lws_add_timespec_delta(&par->time_self, &par->time_self_start, &time);
		lws_add_memory_delta(&par->memory, par->memory_start, memory);
		if (ar->event == LUA_HOOKTAILCALL || ar->event == LUA_HOOKRET) {
			par->depth--;
			if (par->depth == 0) {
				lws_add_timespec_delta(&par->time_total, &par->time_total_start,
						&time);
			}
			p->stack_n--;
		}
	}

	/* transition */
	if (ar->event == LUA_HOOKCALL || ar->event == LUA_HOOKTAILCALL) {
		/* identify function */
		lua_getinfo(L, "nS", ar);
		if (ar->name) {
			name = (u_char *)ar->name;
		} else if (*ar->what == 'm') {
			name = (u_char *)"main chunk";
		} else if (*ar->what == 'L') {
			name = (u_char *)"anonymous function";
		} else {
			name = (u_char *)"?";
		}
		if (ar->linedefined > 0) {
			last = ngx_slprintf(buf, buf + sizeof(buf), "%s:%d: %s", ar->short_src,
					ar->linedefined, name);
		} else {
			last = ngx_slprintf(buf, buf + sizeof(buf), "%s: %s", ar->short_src, name);
		}
		key.data = buf;
		key.len = last - buf;

		/* get or create activation record */
		par = lws_table_get(p->functions, &key);
		if (!par) {
			par = ngx_calloc(sizeof(lws_activation_record_t), p->log);
			if (!par) {
				luaL_error(L, "failed to allocate profiler activation record");
			}
			if (lws_table_set(p->functions, &key, par) != 0) {
				ngx_free(par);
				luaL_error(L, "failed to set profiler activation record");
			}
		}

		/* grow stack as needed */
		if (p->stack_n == p->stack_alloc) {
			stack_alloc_new = p->stack_alloc * 2;
			stack_new = ngx_alloc(stack_alloc_new * sizeof(lws_activation_record_t *),
					p->log);
			if (!stack_new) {
				luaL_error(L, "failed to grow profiler stack");
			}
			ngx_memcpy(stack_new, p->stack, p->stack_n
					* sizeof(lws_activation_record_t *));
			ngx_free(p->stack);
			p->stack = stack_new;
			p->stack_alloc = stack_alloc_new;
		}

		/* push activation record */
		p->stack[p->stack_n] = par;
		p->stack_n++;
	} else {
		/* return */
		par = p->stack_n > 0 ? p->stack[p->stack_n - 1] : NULL;
	}

	/* process function entry */
	if (par) {
		if (clock_gettime(p->clock, &time) != 0) {
			luaL_error(L, "failed to get profiler %s time",
					lws_profiler_state_names[p->state]);
		}
		par->time_self_start = time;
		par->memory_start = memory;
		if (ar->event == LUA_HOOKCALL || ar->event == LUA_HOOKTAILCALL) {
			if (par->depth == 0) {
				par->time_total_start = time;
			}
			par->depth++;
			par->calls++;
		}
	}
}

int lws_open_profiler (lua_State *L) {
	/* profiler */
	luaL_newmetatable(L, LWS_PROFILER);
	lua_pushcfunction(L, lws_profiler_tostring);
	lua_setfield(L, -2, "__tostring");
	lua_pushcfunction(L, lws_profiler_gc);
	lua_setfield(L, -2, "__gc");
	lua_pop(L, 1);

	return 0;
}

int lws_start_profiler (lua_State *L) {
	lws_profiler_t     *p;
	lws_request_ctx_t  *ctx;

	/* create and set profiler */
	p = lua_newuserdata(L, sizeof(lws_profiler_t));
	ngx_memzero(p, sizeof(lws_profiler_t));
	luaL_getmetatable(L, LWS_PROFILER);
	lua_setmetatable(L, -2);
	lua_setfield(L, LUA_REGISTRYINDEX, LWS_PROFILER_CURRENT);

	/* initialize profiler */
	ctx = (void *)lua_topointer(L, 1);
	p->log = ctx->r->connection->log;
	p->functions = lws_table_create(32, p->log);
	if (!p->functions) {
		return luaL_error(L, "failed to create profiler functions");
	}
	lws_table_set_dup(p->functions, 1);
	lws_table_set_free(p->functions, 1);
	p->stack_alloc = 32;
	p->stack = ngx_alloc(p->stack_alloc * sizeof(lws_activation_record_t *), p->log);
	if (!p->stack) {
		return luaL_error(L, "faild to allocate profiler stack");
	}
	p->state = ctx->state->profiler;
	p->clock = p->state == 1 ? LWS_PROFILER_CLOCK_CPU : LWS_PROFILER_CLOCK_WALL;

	/* set hook */
	lua_sethook(L, lws_profiler_hook, LUA_MASKCALL | LUA_MASKRET, 0);
#ifdef NGX_DEBUG
	struct timespec  res;
	if (clock_getres(p->clock, &res) != 0) {
		ngx_log_error(NGX_LOG_ERR, p->log, 0,
				"[LWS] failed to get profiler %s clock resolution",
				lws_profiler_state_names[p->state]);
	} else {
		ngx_log_debug3(NGX_LOG_DEBUG_HTTP, p->log, 0,
				"[LWS] profiler started, clock:%s res:%is%ins",
				lws_profiler_state_names[p->state], (ngx_int_t)res.tv_sec,
				(ngx_int_t)res.tv_nsec);
	}
#endif

	return 0;
}

int lws_stop_profiler (lua_State *L) {
	size_t                    i, functions_alloc_new;
	ngx_str_t                *key;
	lws_function_t           *f, *functions_new;
	lws_profiler_t           *p;
	lws_main_conf_t          *lmcf;
	lws_activation_record_t  *par;

	/* get profiler */
	lua_getfield(L, LUA_REGISTRYINDEX, LWS_PROFILER_CURRENT);
	if (!(p = luaL_testudata(L, -1, LWS_PROFILER))) {
		luaL_error(L, "failed to get profiler");
	}

	/* synchronize profiled functions into monitor */
	lmcf = ngx_http_cycle_get_module_main_conf(ngx_cycle, lws_module);
	ngx_shmtx_lock(&lmcf->monitor_pool->mutex);
	for (i = 0; i < lmcf->monitor->functions_n; i++) {
		f = &lmcf->monitor->functions[i];
		par = lws_table_get(p->functions, &f->key);
		if (par) {
			/* update existing */
			f->calls += par->calls;
			lws_add_timespec(&f->time_self, &par->time_self);
			lws_add_timespec(&f->time_total, &par->time_total);
			f->memory += par->memory;
			lws_table_set(p->functions, &f->key, NULL);  /* to avoid adding */
		}
	}
	if (!lmcf->monitor->out_of_memory) {
		key = NULL;
		while (lws_table_next(p->functions, key, &key, (void**)&par) == 0) {
			/* add new */
			if (lmcf->monitor->functions_n == lmcf->monitor->functions_alloc) {
				functions_alloc_new = lmcf->monitor->functions_alloc * 2;
				functions_new = ngx_slab_alloc_locked(lmcf->monitor_pool,
						functions_alloc_new * sizeof(lws_function_t));
				if (!functions_new) {
					ngx_log_error(NGX_LOG_ERR, p->log, 0, "[LWS] "
							"failed to allocate monitor functions");
					lmcf->monitor->out_of_memory = 1;
					break;
				}
				ngx_memcpy(functions_new, lmcf->monitor->functions,
						lmcf->monitor->functions_n
						* sizeof(lws_function_t));
				ngx_slab_free_locked(lmcf->monitor_pool, lmcf->monitor->functions);
				lmcf->monitor->functions = functions_new;
				lmcf->monitor->functions_alloc = functions_alloc_new;
			}
			f = &lmcf->monitor->functions[lmcf->monitor->functions_n];
			f->key.data = ngx_slab_alloc_locked(lmcf->monitor_pool, key->len);
			if (!f->key.data) {
				ngx_log_error(NGX_LOG_ERR, p->log, 0,
						"[LWS] failed to allocate monitor function key");
				lmcf->monitor->out_of_memory = 1;
				break;
			}
			ngx_memcpy(f->key.data, key->data, key->len);
			f->key.len = key->len;
			f->calls = par->calls;
			f->time_self = par->time_self;
			f->time_total = par->time_total;
			f->memory = par->memory;
			lmcf->monitor->functions_n++;
		}
	}
	ngx_shmtx_unlock(&lmcf->monitor_pool->mutex);

	/* clear hook */
	lua_sethook(L, NULL, 0, 0);

	/* clear profiler */
	lua_pushnil(L);
	lua_setfield(L, LUA_REGISTRYINDEX, LWS_PROFILER_CURRENT);
	ngx_log_debug0(NGX_LOG_DEBUG_HTTP, p->log, 0, "[LWS] profiler stopped");

	return 0;
}
