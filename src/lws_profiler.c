/*
 * LWS profiler
 *
 * Copyright (C) 2023 Andre Naef
 */


#include <lws_profiler.h>
#include <lauxlib.h>


static int lws_profiler_tostring(lua_State *L);
static int lws_profiler_gc(lua_State *L);
static inline void lws_timespec_add_delta(struct timespec *base, const struct timespec *from,
		const struct timespec *to);
static inline void lws_memory_add_delta(size_t *base, size_t from, size_t to);
static void lws_profiler_hook(lua_State *L, lua_Debug *ar);


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
	if (p->stack) {
		ngx_free(p->stack);
	}
	return 0;
}

static inline void lws_timespec_add_delta (struct timespec *base, const struct timespec *from,
		const struct timespec *to) {
	base->tv_nsec += to->tv_nsec - from->tv_nsec;
	base->tv_sec += to->tv_sec - from->tv_sec;
	if (base->tv_nsec < 0) {
		base->tv_nsec += 1000000000;
		base->tv_sec--;
	}
}

static inline void lws_memory_add_delta (size_t *base, size_t from, size_t to) {
	if (to > from) {
		*base += to - from;
	}
}

static void lws_profiler_hook (lua_State *L, lua_Debug *ar) {
	u_char                    buf[LWS_PROFILER_KEY_MAX + 1], *f, *last;
	size_t                    memory, stack_alloc_new;
	ngx_str_t                 key;
	lws_profiler_t           *p;
	struct timespec           time;
	lws_activation_record_t  *par, **stack_new;

	/* get time and memory on entry */
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) != 0) {
		luaL_error(L, "failed to get profiler thread CPU time");
	}
	memory = (size_t)lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0);

	/* get profiler */
	lua_getfield(L, LUA_REGISTRYINDEX, LWS_PROFILER_CURRENT);
	if (!(p = luaL_testudata(L, -1, LWS_PROFILER))) {
		luaL_error(L, "failed to get profiler");
	}
	lua_pop(L, 1);

	/* process exit */
	if (p->stack_n > 0) {
		par = p->stack[p->stack_n - 1];
		lws_timespec_add_delta(&par->time_self, &par->time_self_start, &time);
		lws_memory_add_delta(&par->memory, par->memory_start, memory);
		if (ar->event == LUA_HOOKTAILCALL || ar->event == LUA_HOOKRET) {
			par->depth--;
			if (par->depth == 0) {
				lws_timespec_add_delta(&par->time_total, &par->time_total_start,
						&time);
			}
			p->stack_n--;
		}
	}

	/* transition */
	if (ar->event == LUA_HOOKCALL || ar->event == LUA_HOOKTAILCALL) {
		/* identify function */
		lua_getinfo(L, "nS", ar);
		f = ngx_cpystrn(buf, (u_char *)ar->short_src, sizeof(buf));
		last = buf + sizeof(buf);
		if (ar->linedefined > 0) {
			f = ngx_slprintf(f, last, ":%d", ar->linedefined);
		}
		f = ngx_cpystrn(f, (u_char *)": ", last - f);
		if (ar->name) {
			f = ngx_cpystrn(f, (u_char *)ar->name, last - f);
		} else if (*ar->what == 'm') {
			f = ngx_cpystrn(f, (u_char *)"main chunk", last - f);
		} else if (*ar->what == 'L') {
			f = ngx_cpystrn(f, (u_char *)"anonymous function", last - f);
		} else {
			f = ngx_cpystrn(f, (u_char *)"?", last - f);
		}
		key.data = buf;
		key.len = f - buf;

		/* get or create activation record */
		par = lws_table_get(p->functions, &key);
		if (!par) {
			par = ngx_calloc(sizeof(lws_activation_record_t), ngx_cycle->log);
			if (!par) {
				luaL_error(L, "failed to allocate profiler activation record");
			}
			if (lws_table_set(p->functions, &key, par) != 0) {
				luaL_error(L, "failed to set profiler activation record");
			}
		}

		/* grow stack as needed */
		if (p->stack_n == p->stack_alloc) {
			stack_alloc_new = p->stack_alloc * 2;
			stack_new = ngx_alloc(stack_alloc_new * sizeof(lws_activation_record_t *),
					ngx_cycle->log);
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

	/* process entry */
	if (par) {
		if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) != 0) {
			luaL_error(L, "failed to get profiler thread CPU time");
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

int lws_profiler_open (lua_State *L) {
	/* profiler */
	luaL_newmetatable(L, LWS_PROFILER);
	lua_pushcfunction(L, lws_profiler_tostring);
	lua_setfield(L, -2, "__tostring");
	lua_pushcfunction(L, lws_profiler_gc);
	lua_setfield(L, -2, "__gc");
	lua_pop(L, 1);

	return 0;
}

int lws_profiler_start (lua_State *L) {
	lws_profiler_t  *p;

	/* create and set profiler */
	p = lua_newuserdata(L, sizeof(lws_profiler_t));
	ngx_memzero(p, sizeof(lws_profiler_t));
	luaL_getmetatable(L, LWS_PROFILER);
	lua_setmetatable(L, -2);
	lua_setfield(L, LUA_REGISTRYINDEX, LWS_PROFILER_CURRENT);

	/* initialize profiler */
	p->functions = lws_table_create(32);
	if (!p->functions) {
		return luaL_error(L, "failed to create profiler functions");
	}
	lws_table_set_dup(p->functions, 1);
	lws_table_set_free(p->functions, 1);
	p->stack_alloc = 32;
	p->stack = ngx_alloc(p->stack_alloc * sizeof(lws_activation_record_t *), ngx_cycle->log);
	if (!p->stack) {
		return luaL_error(L, "faild to allocate profiler stack");
	}

	/* set hook */
	lua_sethook(L, lws_profiler_hook, LUA_MASKCALL | LUA_MASKRET, 0);

	return 0;
}

int lws_profiler_stop (lua_State *L) {
	/* clear hook */
	lua_sethook(L, NULL, 0, 0);

	/* clear profiler */
	lua_pushnil(L);
	lua_setfield(L, LUA_REGISTRYINDEX, LWS_PROFILER_CURRENT);

	return 0;
}
