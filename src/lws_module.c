/*
 * LWS module
 *
 * Copyright (C) 2023 Andre Naef
 */


#include <ngx_thread_pool.h>
#include <lws_module.h>


static void *lws_create_main_conf(ngx_conf_t *cf);
static char *lws_init_main_conf(ngx_conf_t *cf, void *main);
static void lws_cleanup_main_conf(void *data);
static char *lws_conf_set_stat_cache(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void *lws_create_loc_conf(ngx_conf_t *cf);
static char *lws_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static void lws_cleanup_loc_conf(void *data);
static char *lws_conf_set_lws(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *lws_conf_set_max_states(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *lws_conf_set_variable(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *lws_conf_set_error_response(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static lws_file_status_e lws_get_file_status(ngx_http_request_t *t, ngx_str_t *filename);
static int lws_set_header(lws_table_t *t, ngx_http_request_t *r, ngx_table_elt_t *header);
static ngx_int_t lws_handler(ngx_http_request_t *r);
static void lws_handler_continuation(ngx_http_request_t *r);
static void lws_handler_state(lws_request_ctx_t *ctx);
static void lws_handler_thread(void *data, ngx_log_t *log);
static ssize_t lws_request_read(void *cookie, char *buf, size_t size);
static void lws_handler_completion(ngx_event_t *ev);
static void lws_send_error_response(lws_request_ctx_t *ctx, ngx_int_t rc);
static void lws_send_json_error_response(lws_request_ctx_t *ctx, ngx_int_t rc);
static void lws_send_html_error_response(lws_request_ctx_t *ctx, ngx_int_t rc);
static void lws_cleanup_request_ctx (void *data);


static cookie_io_functions_t lws_request_read_functions = {
	lws_request_read,  /* read */
	NULL,              /* write */
	NULL,              /* seek */
	NULL               /* close */
};

static ngx_conf_enum_t lws_error_responses[] = {
	{ ngx_string("json"), LWS_ER_JSON },
	{ ngx_string("html"), LWS_ER_HTML },
	{ ngx_null_string, 0 }
};

static ngx_command_t lws_commands[] = {
	{
		ngx_string("lws"),
		NGX_HTTP_LOC_CONF | NGX_CONF_TAKE12,
		lws_conf_set_lws,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(lws_loc_conf_t, main),
		NULL
	},
	{
		ngx_string("lws_init"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(lws_loc_conf_t, init),
		NULL
	},
	{
		ngx_string("lws_pre"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(lws_loc_conf_t, pre),
		NULL
	},
	{
		ngx_string("lws_post"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(lws_loc_conf_t, post),
		NULL
	},
	{
		ngx_string("lws_path"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(lws_loc_conf_t, path),
		NULL
	},
	{
		ngx_string("lws_cpath"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(lws_loc_conf_t, cpath),
		NULL
	},
	{
		ngx_string("lws_max_states"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE12,
		lws_conf_set_max_states,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(lws_loc_conf_t, states_max),
		NULL
	},
	{
		ngx_string("lws_max_memory"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
		ngx_conf_set_size_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(lws_loc_conf_t, memory_max),
		NULL
	},
	{
		ngx_string("lws_max_requests"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
		ngx_conf_set_num_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(lws_loc_conf_t, requests_max),
		NULL
	},
	{
		ngx_string("lws_max_time"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
		ngx_conf_set_msec_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(lws_loc_conf_t, time_max),
		NULL
	},
	{
		ngx_string("lws_timeout"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
		ngx_conf_set_msec_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(lws_loc_conf_t, timeout),
		NULL
	},
	{
		ngx_string("lws_gc"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
		ngx_conf_set_size_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(lws_loc_conf_t, gc),
		NULL
	},
	{
		ngx_string("lws_variable"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
		lws_conf_set_variable,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(lws_loc_conf_t, variables),
		NULL
	},
	{
		ngx_string("lws_error_response"),
		NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE12,
		lws_conf_set_error_response,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(lws_loc_conf_t, error_response),
		lws_error_responses
	},
	{
		ngx_string("lws_thread_pool"),
		NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(lws_main_conf_t, thread_pool_name),
		NULL
	},
	{
		ngx_string("lws_stat_cache"),
		NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE2,
		lws_conf_set_stat_cache,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(lws_main_conf_t, stat_cache_cap),
		NULL
	},
	ngx_null_command
};

static ngx_http_module_t lws_ctx = {
	NULL,                  /* preconfiguration */
	NULL,                  /* postconfiguration */
	lws_create_main_conf , /* create main configuration */
	lws_init_main_conf,    /* init main configuration */
	NULL,                  /* create server configuration */
	NULL,                  /* merge server configuration */
	lws_create_loc_conf,   /* create location configuration */
	lws_merge_loc_conf     /* merge location configuration */
};

ngx_module_t lws = {
	NGX_MODULE_V1,
	&lws_ctx,
	lws_commands,
	NGX_HTTP_MODULE,
	NULL,                  /* init master */
	NULL,                  /* init module */
	NULL,                  /* init process */
	NULL,                  /* init thread */
	NULL,                  /* exit thread */
	NULL,                  /* exit process */
	NULL,                  /* exit master */
	NGX_MODULE_V1_PADDING
};


/*
 * configuration
 */

static void *lws_create_main_conf (ngx_conf_t *cf) {
	lws_main_conf_t     *lmcf;
	ngx_pool_cleanup_t  *cln;

	/* allocate and intialize structure */
	lmcf = ngx_pcalloc(cf->pool, sizeof(lws_main_conf_t));
	if (!lmcf) {
		return NULL;
	}
	lmcf->stat_cache_cap = NGX_CONF_UNSET_SIZE;
	lmcf->stat_cache_timeout = NGX_CONF_UNSET;

	/* add cleanup */
	cln = ngx_pool_cleanup_add(cf->pool, 0);
	if (!cln) {
		return NGX_CONF_ERROR;
	}
	cln->handler = lws_cleanup_main_conf;
	cln->data = lmcf;

	return lmcf;
}

static char *lws_init_main_conf (ngx_conf_t *cf, void *conf) {
	lws_main_conf_t  *lmcf;

	/* thread pool */
	lmcf = conf;
	if (!lmcf->thread_pool_name.len) {
		ngx_str_set(&lmcf->thread_pool_name, LWS_THREAD_POOL_NAME_DEFAULT);
	}
	lmcf->thread_pool = ngx_thread_pool_add(cf, &lmcf->thread_pool_name);
	if (!lmcf->thread_pool) {
		return NGX_CONF_ERROR;
	}

	/* stat cache */
	ngx_conf_init_size_value(lmcf->stat_cache_cap, LWS_STAT_CACHE_CAP_DEFAULT);
	ngx_conf_init_value(lmcf->stat_cache_timeout, LWS_STAT_CACHE_TIMEOUT_DEFAULT);
	if (lmcf->stat_cache_cap) {
		lmcf->stat_cache = lws_table_create(32);
		if (!lmcf->stat_cache) {
			return NGX_CONF_ERROR;
		}
		lws_table_set_dup(lmcf->stat_cache, 1);
		lws_table_set_cap(lmcf->stat_cache, lmcf->stat_cache_cap);
		lws_table_set_timeout(lmcf->stat_cache, lmcf->stat_cache_timeout);
	}

	return NGX_CONF_OK;
}

static void lws_cleanup_main_conf (void *data) {
	lws_main_conf_t  *lmcf;

	lmcf = data;
	if (lmcf->stat_cache) {
		lws_table_free(lmcf->stat_cache);
	}
}

static char *lws_conf_set_stat_cache (ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
	ngx_str_t        *values;
	lws_main_conf_t  *lmcf;

	lmcf = conf;
	values = cf->args->elts;
	if (lmcf->stat_cache_cap != NGX_CONF_UNSET_SIZE) {
		return "is duplicate";
	}
	lmcf->stat_cache_cap = ngx_parse_size(&values[1]);
	if (lmcf->stat_cache_cap == (size_t)NGX_ERROR) {
		return "has invalid size value";
	}
	lmcf->stat_cache_timeout = ngx_parse_time(&values[2], 1);
	if (lmcf->stat_cache_timeout == (time_t)NGX_ERROR) {
		return "has invalid time value";
	}
	return NGX_CONF_OK;
}

static void *lws_create_loc_conf (ngx_conf_t *cf) {
	lws_loc_conf_t      *llcf;
	ngx_pool_cleanup_t  *cln;

	/* allocate and intialize structure */
	llcf = ngx_pcalloc(cf->pool, sizeof(lws_loc_conf_t));
	if (!llcf) {
		return NULL;
	}
	llcf->states_max = NGX_CONF_UNSET_SIZE;
	llcf->queue_max = NGX_CONF_UNSET_SIZE;
	llcf->memory_max = NGX_CONF_UNSET_SIZE;
	llcf->requests_max = NGX_CONF_UNSET;
	llcf->time_max = NGX_CONF_UNSET_MSEC;
	llcf->timeout = NGX_CONF_UNSET_MSEC;
	llcf->gc = NGX_CONF_UNSET_SIZE;
	llcf->error_response = NGX_CONF_UNSET_UINT;
	llcf->diagnostic = NGX_CONF_UNSET;
	if (ngx_array_init(&llcf->variables, cf->pool, 4, sizeof(lws_variable_t)) != NGX_OK) {
		return NULL;
	}
	ngx_queue_init(&llcf->states);
	ngx_queue_init(&llcf->requests);

	/* add cleanup */
	cln = ngx_pool_cleanup_add(cf->pool, 0);
	if (!cln) {
		return NULL;
	}
	cln->handler = lws_cleanup_loc_conf;
	cln->data = llcf;

	return llcf;
}

static char *lws_merge_loc_conf (ngx_conf_t *cf, void *parent, void *child) {
	lws_loc_conf_t  *prev = parent;
	lws_loc_conf_t  *conf = child;

	if (!conf->main) {
		conf->main = prev->main;
	}
	if (!conf->path_info) {
		conf->path_info = prev->path_info;
	}
	ngx_conf_merge_str_value(conf->init, prev->init, "");
	ngx_conf_merge_str_value(conf->pre, prev->pre, "");
	ngx_conf_merge_str_value(conf->post, prev->post, "");
	ngx_conf_merge_str_value(conf->path, prev->path, "");
	ngx_conf_merge_str_value(conf->cpath, prev->cpath, "");
	ngx_conf_merge_size_value(conf->states_max, prev->states_max, 0);
	ngx_conf_merge_size_value(conf->queue_max, prev->queue_max, 0);
	ngx_conf_merge_size_value(conf->memory_max, prev->memory_max, 0);
	ngx_conf_merge_value(conf->requests_max, prev->requests_max, 0);
	ngx_conf_merge_msec_value(conf->time_max, prev->time_max, 0);
	ngx_conf_merge_msec_value(conf->timeout, prev->timeout, 0);
	ngx_conf_merge_size_value(conf->gc, prev->gc, 0);
	if (!ngx_array_push_n(&conf->variables, prev->variables.nelts)) {
		return NGX_CONF_ERROR;
	}
	ngx_memmove((u_char *)conf->variables.elts + prev->variables.nelts * conf->variables.size,
			conf->variables.elts, (conf->variables.nelts - prev->variables.nelts)
			* conf->variables.size);
	ngx_memcpy(conf->variables.elts, prev->variables.elts, prev->variables.nelts
			* conf->variables.size);
	ngx_conf_merge_uint_value(conf->error_response, prev->error_response, 0);
	ngx_conf_merge_value(conf->diagnostic, prev->diagnostic, 0);
	return NGX_CONF_OK;
}

static void lws_cleanup_loc_conf (void *data) {
	lws_state_t     *state;
	ngx_queue_t     *q;
	lws_loc_conf_t  *llcf;

	llcf = data;
	while (!ngx_queue_empty(&llcf->states)) {
		q = ngx_queue_head(&llcf->states);
		ngx_queue_remove(q);
		state = ngx_queue_data(q, lws_state_t, queue);
		lws_close_state(state, ngx_cycle->log);
	}
}

static char *lws_conf_set_lws (ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
	ngx_str_t                        *values;
	lws_loc_conf_t                   *llcf;
	ngx_http_core_loc_conf_t         *clcf;
	ngx_http_compile_complex_value_t  ccv;

	/* set main */
	llcf = conf;
	if (llcf->main) {
		return "is duplicate";
	}
	llcf->main = ngx_palloc(cf->pool, sizeof(ngx_http_complex_value_t));
	if (!llcf->main) {
		return NGX_CONF_ERROR;
	}
	values = cf->args->elts;
	ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));
	ccv.cf = cf;
	ccv.value = &values[1];
	ccv.complex_value = llcf->main;
	ccv.zero = 1;
	if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
		return NGX_CONF_ERROR;
	}

	/* set optional path info */
	if (cf->args->nelts >= 3) {
		llcf->path_info = ngx_palloc(cf->pool, sizeof(ngx_http_complex_value_t));
		if (!llcf->path_info) {
			return NGX_CONF_ERROR;
		}
		ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));
		ccv.cf = cf;
		ccv.value = &values[2];
		ccv.complex_value = llcf->path_info;
		if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
			return NGX_CONF_ERROR;
		}
	}

	/* install handler */
	clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
	clcf->handler = lws_handler;
	return NGX_CONF_OK;
}

static char *lws_conf_set_max_states (ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
	ngx_str_t       *values;
	lws_loc_conf_t  *llcf;

	values = cf->args->elts;
	llcf = conf;
	if (llcf->states_max != NGX_CONF_UNSET_SIZE) {
		return "is duplicate";
	}
	if ((llcf->states_max = ngx_parse_size(&values[1])) == (size_t)NGX_ERROR) {
		return "has invalid states value";
	}
	if (cf->args->nelts >= 3) {
		if ((llcf->queue_max = ngx_parse_size(&values[2])) == (size_t)NGX_ERROR) {
			return "has invalid queue value";
		}
	}
	return NGX_CONF_OK;
}

static char *lws_conf_set_variable (ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
	ngx_str_t       *values;
	ngx_int_t        index;
	lws_loc_conf_t  *llcf;
	lws_variable_t  *variable;

	/* check argument */
	values = cf->args->elts;
	if ((index = ngx_http_get_variable_index(cf, &values[1])) == NGX_ERROR) {
		return "has invalid variable name";
	}

	/* push variable */
	llcf = conf;
	variable = ngx_array_push(&llcf->variables);
	if (!variable) {
		return NGX_CONF_ERROR;
	}
	variable->name = values[1];
	variable->index = index;

	return NGX_CONF_OK;
}

static char *lws_conf_set_error_response (ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
	char            *result;
	ngx_str_t       *values;
	ngx_uint_t       i;
	lws_loc_conf_t  *llcf;

	if ((result = ngx_conf_set_enum_slot(cf, cmd, conf)) != NGX_CONF_OK) {
		return result;
	}
	llcf = conf;
	llcf->diagnostic = 0;
	values = cf->args->elts;
	for (i = 2; i < cf->args->nelts; i++) {
		if (ngx_strcasecmp(values[i].data, (u_char *)"diagnostic") == 0) {
			llcf->diagnostic = 1;
		} else {
			ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid attribute value \"%s\"",
					values[i].data);
			return NGX_CONF_ERROR;
		}
	}
	return NGX_CONF_OK;
}


/*
 * handler
 */

static lws_file_status_e lws_get_file_status (ngx_http_request_t *r, ngx_str_t *filename) {
	struct stat        sb;
	lws_main_conf_t   *lmcf;
	lws_file_status_e  fs;

	lmcf = ngx_http_get_module_main_conf(r, lws);
	if (lmcf->stat_cache) {
		fs = (uintptr_t)lws_table_get(lmcf->stat_cache, filename);
		ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
				"[LWS] stat_cache get filename:%V fs:%d", filename, fs);
		if (fs != LWS_FS_UNKNOWN) {
			return fs;
		}
	}
	fs = stat((const char *)filename->data, &sb) == 0
			&& (S_ISREG(sb.st_mode) || S_ISLNK(sb.st_mode))
			? LWS_FS_FOUND : LWS_FS_NOT_FOUND;
	if (lmcf->stat_cache) {
		lws_table_set(lmcf->stat_cache, filename, (void *)fs);
		ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
				"[LWS] stat_cache set filename:%V fs:%d", filename, fs);
	}
	return fs;
}

static int lws_set_header (lws_table_t *t, ngx_http_request_t *r, ngx_table_elt_t *header) {
	size_t      len;
	u_char     *p;
	ngx_log_t  *log;
	ngx_str_t  *existing, *value;

	log = r->connection->log;
	existing = lws_table_get(t, &header->key);
	if (!existing) {
		value = &header->value;
	} else {
		len = existing->len + 2 + header->value.len;
		value = ngx_palloc(r->pool, sizeof(ngx_str_t) + len);
		if (!value) {
			ngx_log_error(NGX_LOG_CRIT, log, 0, "[LWS] failed to allocate header");
			return -1;
		}
		value->len = len;
		value->data = (u_char *)value + sizeof(ngx_str_t);
		p = ngx_cpymem(value->data, existing->data, existing->len);
		p = ngx_cpymem(p, ", ", 2);
		ngx_memcpy(p, header->value.data, header->value.len);
	}
	if (lws_table_set(t, &header->key, value) != 0) {
		ngx_log_error(NGX_LOG_CRIT, log, 0, "[LWS] failed to set header");
		return -1;
	}
	return 0;
}

static ngx_int_t lws_handler (ngx_http_request_t *r) {
	ngx_int_t                   rc;
	ngx_log_t                  *log;
	ngx_str_t                   main;
	ngx_str_t                  *value;
	ngx_uint_t                  i;
	ngx_list_part_t            *part;
	ngx_table_elt_t            *headers;
	lws_loc_conf_t             *llcf;
	lws_variable_t             *variables;
	lws_request_ctx_t          *ctx;;
	ngx_pool_cleanup_t         *cln;
	ngx_http_variable_value_t  *variable_value;

	/* check if enabled */
	llcf = ngx_http_get_module_loc_conf(r, lws);
	if (!llcf->main) {
		return NGX_DECLINED;
	}

	/* check main */
	log = r->connection->log;
	if (ngx_http_complex_value(r, llcf->main, &main) != NGX_OK) {
		ngx_log_error(NGX_LOG_ERR, log, 0, "[LWS] failed to evaluate main filename");
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}
	ngx_log_debug1(NGX_LOG_DEBUG_HTTP, log, 0, "[LWS] main filename:%V", &main);
	if (lws_get_file_status(r, &main) == LWS_FS_NOT_FOUND) {
		return NGX_HTTP_NOT_FOUND;
	}

	/* prepare request context */
	ctx = ngx_pcalloc(r->pool, sizeof(lws_request_ctx_t));
	if (!ctx) {
		ngx_log_error(NGX_LOG_CRIT, log, 0, "[LWS] failed to allocate request context");
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}
	ngx_http_set_ctx(r, ctx, lws);
	cln = ngx_pool_cleanup_add(r->pool, 0);
	if (!cln) {
		ngx_log_error(NGX_LOG_CRIT, log, 0, "[LWS] failed to add request cleanup");
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}
	cln->handler = lws_cleanup_request_ctx;
	cln->data = ctx;
	ctx->r = r;
	ctx->main = main;
	if (llcf->path_info && ngx_http_complex_value(r, llcf->path_info, &ctx->path_info)
			!= NGX_OK) {
		ngx_log_error(NGX_LOG_ERR, log, 0, "[LWS] failed to evaluate path info");
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	/* prepare request variables */
	ctx->variables = lws_table_create(llcf->variables.nelts);
	if (!ctx->variables) {
		ngx_log_error(NGX_LOG_ERR, log, 0, "[LWS] failed to create variables");
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}
	variables = llcf->variables.elts;
	for (i = 0; i < llcf->variables.nelts; i++) {
		variable_value = ngx_http_get_indexed_variable(r, variables[i].index);
		if (!variable_value || !variable_value->valid) {
			continue;
		}
		value = ngx_palloc(r->pool, sizeof(ngx_str_t));
		if (!value) {
			ngx_log_error(NGX_LOG_ERR, log, 0, "[LWS] failed to allocate variable");
			return NGX_HTTP_INTERNAL_SERVER_ERROR;
		}
		value->len = variable_value->len;
		value->data = variable_value->data;
		lws_table_set(ctx->variables, &variables[i].name, value);
	}

	/* prepare request headers */
	ctx->request_headers = lws_table_create(32);
	if (!ctx->request_headers) {
		ngx_log_error(NGX_LOG_CRIT, log, 0, "[LWS] failed to create request headers");
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}
	lws_table_set_ci(ctx->request_headers, 1);
	part = &r->headers_in.headers.part;
	while (part) {
		headers = part->elts;
		for (i = 0; i < part->nelts; i++) {
			if (lws_set_header(ctx->request_headers, r, &headers[i]) != 0) {
				return NGX_HTTP_INTERNAL_SERVER_ERROR;
			}
		}
		part = part->next;
	}

	/* prepare response headers */
	ctx->response_headers = lws_table_create(8);
	if (!ctx->response_headers) {
		ngx_log_error(NGX_LOG_CRIT, log, 0, "[LWS] failed to create response headers");
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}
	lws_table_set_dup(ctx->response_headers, 1);
	lws_table_set_free(ctx->response_headers, 1);
	lws_table_set_ci(ctx->response_headers, 1);
	ctx->status = NGX_HTTP_OK;

	/* prepare response body */
	ctx->response_body = open_memstream((char **)&ctx->response_body_str.data,
			&ctx->response_body_str.len);
	if (!ctx->response_body) {
		ngx_log_error(NGX_LOG_CRIT, log, 0, "[LWS] failed to open response body stream");
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	/* read request body */
	rc = ngx_http_read_client_request_body(r, lws_handler_continuation);
	if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
		return rc;
	}
	return NGX_DONE;
}

static void lws_handler_continuation (ngx_http_request_t *r) {
	ngx_log_t          *log;
	lws_loc_conf_t     *llcf;
	lws_request_ctx_t  *ctx;

	/* prepare request body */
	log = r->connection->log;
	ctx = ngx_http_get_module_ctx(r, lws);
	if (!r->request_body) {
		ngx_log_error(NGX_LOG_ERR, log, errno, "[LWS] missing request body");
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
		return;
	}
	if (r->request_body->temp_file) {
		ctx->request_body = fdopen(r->request_body->temp_file->file.fd, "rb");
	} else {
		ctx->cl = r->request_body->bufs;
		ctx->request_body = fopencookie(ctx, "rb", lws_request_read_functions);
	}
	if (!ctx->request_body) {
		ngx_log_error(NGX_LOG_ERR, log, errno, "[LWS] failed to open request body stream");
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
		return;
	}

	/* get state, queue, or abort */
	llcf = ngx_http_get_module_loc_conf(ctx->r, lws);
	if (!ngx_queue_empty(&llcf->states) || llcf->states_max == 0
			|| llcf->states_n < llcf->states_max) {
		lws_handler_state(ctx);
	} else if (llcf->queue_max == 0 || llcf->requests_n < llcf->queue_max) {
		llcf->requests_n++;
		ngx_log_debug2(NGX_LOG_DEBUG_HTTP, log, 0, "[LWS] request queued max:%z n:%z",
				llcf->queue_max, llcf->requests_n);
		ngx_queue_insert_tail(&llcf->requests, &ctx->queue);
	} else {
		ngx_log_error(NGX_LOG_ERR, log, 0, "[LWS] request queue overflow max:%z n:%z",
				llcf->queue_max, llcf->requests_n);
		ngx_http_finalize_request(r, NGX_HTTP_SERVICE_UNAVAILABLE);
	}
}

static void lws_handler_state (lws_request_ctx_t *ctx) {
	ngx_log_t           *log;
	lws_main_conf_t     *lmcf;
	ngx_thread_task_t   *task;
	ngx_http_request_t  *r;

	/* get state */
	r = ctx->r;
	ctx->state = lws_get_state(ctx);
	if (!ctx->state) {
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
		return;
	}

	/* setup task */
	log = r->connection->log;
	task = ngx_thread_task_alloc(r->pool, sizeof(lws_request_ctx_t *));
	if (!task) {
		lws_put_state(ctx, ctx->state);
		ngx_log_error(NGX_LOG_CRIT, log, 0, "[LWS] failed to allocate thread task");
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
		return;
	}
	*(lws_request_ctx_t **)task->ctx = ctx;
	task->handler = lws_handler_thread;
	task->event.handler = lws_handler_completion;
	task->event.data = ctx;

	/* post task */
	lmcf = ngx_http_get_module_main_conf(r, lws);
	if (ngx_thread_task_post(lmcf->thread_pool, task) != NGX_OK) {
		lws_put_state(ctx, ctx->state);
		ngx_log_error(NGX_LOG_CRIT, log, 0, "[LWS] failed to post thread task");
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
		return;
	}
}

static void lws_handler_thread (void *data, ngx_log_t *log) {
	lws_request_ctx_t  *ctx;

	ctx = *(lws_request_ctx_t **)data;
	ctx->rc = lws_run_state(ctx);
}

static ssize_t lws_request_read (void *cookie, char *buf, size_t size) {
	size_t              count;
	ngx_buf_t          *b;
	lws_request_ctx_t  *ctx;

	/* done? */
	ctx = cookie;
	if (!ctx->cl) {
		return 0;
	}

	/* read */
	b = ctx->cl->buf;
	count = b->last - b->pos;
	if (count > size) {
		count = size;
	}
	memcpy(buf, b->pos, count);
	b->pos += count;
	if (b->pos == b->last) {
		ctx->cl = ctx->cl->next;
	}
	return count;
}

static void lws_handler_completion (ngx_event_t *ev) {
	ngx_buf_t           *b;
	ngx_int_t            rc;
	ngx_log_t           *log;
	ngx_str_t           *key, *value;
	ngx_queue_t         *q;
	ngx_chain_t         *out;
	lws_loc_conf_t      *llcf;
	ngx_table_elt_t     *h;
	lws_request_ctx_t   *ctx, *ctx_next;
	ngx_http_request_t  *r;

	/* get request */
	ctx = ev->data;

	/* put state */
	if (ctx->rc < 0) {
		ctx->state->close = 1;
	}
	lws_put_state(ctx, ctx->state);

	/* check for queued requests */
	r = ctx->r;
	llcf = ngx_http_get_module_loc_conf(r, lws);
	if (!ngx_queue_empty(&llcf->requests)) {
		q = ngx_queue_head(&llcf->requests);
		ngx_queue_remove(q);
		llcf->requests_n--;
		ctx_next = ngx_queue_data(q, lws_request_ctx_t, queue);
		lws_handler_state(ctx_next);
	}

	/* Lua error generated? */
	if (ctx->rc < 0) {
		lws_send_error_response(ctx, NGX_HTTP_INTERNAL_SERVER_ERROR);
		return;
	}

	/* internal redirect? */
	if (ctx->redirect) {
		if (ctx->redirect->data[0] != '@') {
			rc = ngx_http_internal_redirect(r, ctx->redirect, ctx->redirect_args);
		} else {
			ctx->redirect->data++;
			ctx->redirect->len--;
			rc = ngx_http_named_location(r, ctx->redirect);
		}
		ngx_http_finalize_request(r, rc);
		return;
	}

	/* set headers */
	key = NULL;
	while (lws_table_next(ctx->response_headers, key, &key, (void**)&value) == 0) {
		#define lws_is_header(literal)  ngx_strncasecmp(key->data, (u_char *)literal,  \
				 sizeof(literal) - 1) == 0
		if (key->len == 12 && lws_is_header("Content-Type")) {
			r->headers_out.content_type = *value;
			r->headers_out.content_type_len = value->len;
			continue;
		} else if (key->len == 14 && lws_is_header("Content-Length")) {
			continue;  /* content length is handled separately below */
		}
		h = ngx_list_push(&r->headers_out.headers);
		if (!h) {
			ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
			return;
		}
		switch (key->len) {
		case 4:
			if (lws_is_header("Date")) {
				r->headers_out.date = h;
			} else if (lws_is_header("ETag")) {
				r->headers_out.etag = h;
			}
			break;

		case 6:
			if (lws_is_header("Server")) {
				r->headers_out.server = h;
			}
			break;

		case 7:
			if (lws_is_header("Refresh")) {
				r->headers_out.refresh = h;
			} else if (lws_is_header("Expires")) {
				r->headers_out.expires = h;
			}
			break;

		case 8:
			if (lws_is_header("Location")) {
				r->headers_out.location = h;
			}
			break;

		case 13:
			if (lws_is_header("Last-Modified")) {
				r->headers_out.last_modified = h;
			} else if (lws_is_header("Content-Range")) {
				r->headers_out.content_range = h;
			} else if (lws_is_header("Accept-Ranges")) {
				r->headers_out.accept_ranges = h;
			}
			break;

		case 16:
			if (lws_is_header("Content-Encoding")) {
				r->headers_out.content_encoding = h;
			} else if (lws_is_header("WWW-Authenticate")) {
				r->headers_out.www_authenticate = h;
			}
			break;
		}
		#undef lws_is_header
		h->key = *key;
		h->value = *value;
		h->hash = 1;
	}

	/* error response requested? */
	if (ctx->rc > 0) {
		rc = ctx->rc >= 100 && ctx->rc < 600 ? ctx->rc : NGX_HTTP_INTERNAL_SERVER_ERROR;
		lws_send_error_response(ctx, rc);
		return;
	}

	/* send headers */
	log = r->connection->log;
	r->headers_out.status = ctx->status;
	if (fflush(ctx->response_body) != 0) {
		ngx_log_error(NGX_LOG_CRIT, log, errno, "[LWS] failed to flush response body");;
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
		return;
	}
	if (ctx->response_body_str.len > 0) {
		if (r == r->main && (r->method == NGX_HTTP_HEAD
				|| r->headers_out.status == NGX_HTTP_NO_CONTENT
				|| r->headers_out.status == NGX_HTTP_NOT_MODIFIED)) {
			/* body found, but filter modules would flag as header-only */
			ngx_log_error(NGX_LOG_WARN, log, 0, "[LWS] ignoring response body");
			r->header_only = 1;
		} else {
			r->headers_out.content_length_n = ctx->response_body_str.len;
		}
	} else {
		if (r == r->main && (r->method != NGX_HTTP_HEAD
				&& r->headers_out.status != NGX_HTTP_NO_CONTENT
				&& r->headers_out.status != NGX_HTTP_NOT_MODIFIED
				&& r->headers_out.status >= NGX_HTTP_OK)) {
			/* no body, but filter modules would trigger chunked transfer */
			ngx_log_error(NGX_LOG_WARN, log, 0, "[LWS] response body expected");
			r->headers_out.content_length_n = 0;
			r->header_only = 1;
		}
	}
	rc = ngx_http_send_header(r);
	if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
		ngx_http_finalize_request(r, rc);
		return;
	}

	/* send body */
	out = ngx_alloc_chain_link(r->pool);
	b = ngx_calloc_buf(r->pool);
	if (!out || !b) {
		ngx_http_finalize_request(r, rc);
		return;
	}
	b->start = ctx->response_body_str.data;
	b->pos = b->start;
	b->end = ctx->response_body_str.data + ctx->response_body_str.len;
	b->last = b->end;
	b->temporary = 1;
	b->last_buf = (r == r->main) ? 1 : 0;
	b->last_in_chain = 1;
	out->buf = b;
	out->next = NULL;
	rc = ngx_http_output_filter(r, out);
	ngx_free_chain(r->pool, out);
	ngx_http_finalize_request(r, rc);
}

static void lws_send_error_response (lws_request_ctx_t *ctx, ngx_int_t rc) {
	lws_loc_conf_t      *llcf;
	ngx_http_request_t  *r;

	/* check for header-only requests */
	r = ctx->r;
	if (r == r->main && (r->method == NGX_HTTP_HEAD
			|| rc == NGX_HTTP_NO_CONTENT
			|| rc == NGX_HTTP_NOT_MODIFIED)) {
		r->header_only = 1;
		r->headers_out.status = rc;
		ngx_http_finalize_request(r, ngx_http_send_header(r));
		return;
	}

	/* send full error response */
	llcf = ngx_http_get_module_loc_conf(r, lws);
	switch (llcf->error_response) {
	case LWS_ER_JSON:
		lws_send_json_error_response(ctx, rc);
		break;

	case LWS_ER_HTML:
		lws_send_html_error_response(ctx, rc);
		break;

	default:
		lws_send_json_error_response(ctx, rc);
	}
}

static void lws_send_json_error_response (lws_request_ctx_t *ctx, ngx_int_t rc) {
	size_t              len;
	ngx_str_t           dia;
	ngx_buf_t           *b;
	ngx_chain_t         *out;
	ngx_table_elt_t     *h;
	lws_http_status_t   *status;
	ngx_http_request_t  *r;

	/* calculate length */
	r = ctx->r;
	len = sizeof("{\n\t\"error\": {\n\t\t\"code\": 100\n\t}\n}") - 1;
	status = lws_http_find_status(rc);
	if (status) {
		len += sizeof(",\n\t\t\"message\": \"\"") - 1 + status->message.len;
	}
	if (ctx->diagnostic) {
		dia.len = ctx->diagnostic->len;
		dia.len += ngx_escape_json(NULL, ctx->diagnostic->data, ctx->diagnostic->len);
		dia.data = ngx_palloc(r->pool, dia.len);
		if (!dia.data) {
			ngx_http_finalize_request(r, rc);
			return;
		}
		ngx_escape_json(dia.data, ctx->diagnostic->data, ctx->diagnostic->len);
		len += sizeof(",\n\t\t\"diagnostic\": \"\"") - 1 + dia.len;
	} else {
		dia.len = 0;
	}

	/* allocate buffer */
	out = ngx_alloc_chain_link(r->pool);
	b = ngx_calloc_buf(r->pool);
	if (!out || !b) {
		ngx_http_finalize_request(r, rc);
		return;
	}
	b->start = ngx_palloc(r->pool, len);
	if (!b->start) {
		ngx_http_finalize_request(r, rc);
		return;
	}
	b->pos = b->start;
	b->end = b->start + len;
	b->last = b->pos;

	/* build JSON */
	b->last = ngx_cpymem(b->last, "{\n\t\"error\": {", sizeof("{\n\t\"error\": {") - 1);
	b->last = ngx_sprintf(b->last, "\n\t\t\"code\": %ui", rc);
	if (status) {
		b->last = ngx_sprintf(b->last, ",\n\t\t\"message\": \"%V\"", &status->message);
	}
	if (dia.len) {
		b->last = ngx_sprintf(b->last, ",\n\t\t\"diagnostic\": \"%V\"", &dia);
	}
	b->last = ngx_cpymem(b->last, "\n\t}\n}", sizeof("\n\t}\n}") - 1);

	/* send headers */
	r->headers_out.status = rc;
	ngx_str_set(&r->headers_out.content_type, "application/json");
	r->headers_out.content_type_len = r->headers_out.content_type.len;
	h = ngx_list_push(&r->headers_out.headers);
	if (!h) {
		ngx_http_finalize_request(r, rc);
		return;
	}
	ngx_str_set(&h->key, "Cache-Control");
	ngx_str_set(&h->value, "private, no-store");
	h->hash = 1;
	r->headers_out.content_length_n = len;
	rc = ngx_http_send_header(r);
	if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
		ngx_http_finalize_request(r, rc);
		return;
	}

	/* send body */
	b->temporary = 1;
	b->last_buf = (r == r->main) ? 1 : 0;
	b->last_in_chain = 1;
	out->buf = b;
	out->next = NULL;
	rc = ngx_http_output_filter(r, out);
	ngx_free_chain(r->pool, out);
	ngx_http_finalize_request(r, rc);
}

static void lws_send_html_error_response (lws_request_ctx_t *ctx, ngx_int_t rc) {
	ngx_str_t            pre, epi, dia;
	ngx_buf_t           *b_pre, *b_dia, *b_epi;
	ngx_chain_t         *out_pre, *out_dia, *out_epi;
	ngx_table_elt_t     *h;
	ngx_http_request_t  *r;

	/* no diagnostic? use server error pages */
	r = ctx->r;
	if (!ctx->diagnostic) {
		ngx_http_finalize_request(r, rc);
		return;
	}

	/* set preamble and epilogue */
	ngx_str_set(&pre, "<!DOCTYPE HTML>\n"
		"<html>\n"
		"\t<head>\n"
		"\t\t<title>LWS " LUA_VERSION " Error</title>\n"
		"\t\t<style>\n"
		"\t\t\thtml {\n"
		"\t\t\t\tfont-family: system-ui, \"Segoe UI\", -apple-system, "
			"\"Liberation Sans\", Roboto, \"Helvetica Neue\", Arial, sans-serif;\n"
		"\t\t\t}\n"
		"\t\t\tbody {\n"
		"\t\t\t\tmax-width: 600px;\n"
		"\t\t\t\tmargin: 0 auto;\n"
		"\t\t\t}\n"
		"\t\t</style>\n"
		"\t</head>\n"
		"\t<body>\n"
		"\t\t<h2>LWS " LUA_VERSION " Error</h2>\n"
		"\t\t<pre>");
	ngx_str_set(&epi, "</pre>\n"
		"\t</body>\n"
		"</html>");

	/* escape message */
	dia.len = ctx->diagnostic->len;
	dia.len += ngx_escape_html(NULL, ctx->diagnostic->data, ctx->diagnostic->len);
	dia.data = ngx_palloc(r->pool, dia.len);
	if (!dia.data) {
		ngx_http_finalize_request(r, rc);
		return;
	}
	ngx_escape_html(dia.data, ctx->diagnostic->data, ctx->diagnostic->len);

	/* send headers */
	r->headers_out.status = rc;
	ngx_str_set(&r->headers_out.content_type, "text/html");
	r->headers_out.content_type_len = r->headers_out.content_type.len;
	ngx_str_set(&r->headers_out.charset, "UTF-8");
	h = ngx_list_push(&r->headers_out.headers);
	if (!h) {
		ngx_http_finalize_request(r, rc);
		return;
	}
	ngx_str_set(&h->key, "Cache-Control");
	ngx_str_set(&h->value, "private, no-store");
	h->hash = 1;
	r->headers_out.content_length_n = pre.len + dia.len + epi.len;
	rc = ngx_http_send_header(r);
	if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
		ngx_http_finalize_request(r, rc);
		return;
	}

	/* send body */
	out_pre = ngx_alloc_chain_link(r->pool);
	b_pre = ngx_calloc_buf(r->pool);
	out_dia = ngx_alloc_chain_link(r->pool);
	b_dia = ngx_calloc_buf(r->pool);
	out_epi = ngx_alloc_chain_link(r->pool);
	b_epi = ngx_calloc_buf(r->pool);
	if (!out_pre || !b_pre || !out_dia || !b_dia || !out_epi || !b_epi) {
		ngx_http_finalize_request(r, rc);
		return;
	}
	b_pre->start = pre.data;
	b_pre->pos = b_pre->start;
	b_pre->end = pre.data + pre.len;
	b_pre->last = b_pre->end;
	b_pre->memory = 1;
	out_pre->buf = b_pre;
	out_pre->next = out_dia;
	b_dia->start = dia.data;
	b_dia->pos = b_dia->start;
	b_dia->end = dia.data + dia.len;
	b_dia->last = b_dia->end;
	b_dia->temporary = 1;
	out_dia->buf = b_dia;
	out_dia->next = out_epi;
	b_epi->start = epi.data;
	b_epi->pos = b_epi->start;
	b_epi->end = epi.data + epi.len;
	b_epi->last = b_epi->end;
	b_epi->memory = 1;
	b_epi->last_buf = (r == r->main) ? 1 : 0;
	b_epi->last_in_chain = 1;
	out_epi->buf = b_epi;
	out_epi->next = NULL;
	rc = ngx_http_output_filter(r, out_pre);
	ngx_free_chain(r->pool, out_pre);
	ngx_free_chain(r->pool, out_dia);
	ngx_free_chain(r->pool, out_epi);
	ngx_http_finalize_request(r, rc);
}

static void lws_cleanup_request_ctx (void *data) {
	lws_request_ctx_t  *ctx;

	ctx = data;
	if (ctx->variables) {
		lws_table_free(ctx->variables);
	}
	if (ctx->request_headers) {
		lws_table_free(ctx->request_headers);
	}
	if (ctx->request_body) {
		fclose(ctx->request_body);
	}
	if (ctx->response_headers) {
		lws_table_free(ctx->response_headers);
	}
	if (ctx->response_body) {
		fclose(ctx->response_body);
		free(ctx->response_body_str.data);
	}
	ngx_free(ctx->redirect);
	ngx_free(ctx->redirect_args);
	ngx_free(ctx->diagnostic);
}
