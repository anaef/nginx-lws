/*
 * LWS monitor
 *
 * Copyright (C) 2023 Andre Naef
 */


#include <lws_monitor.h>
#include <lws_module.h>


static ngx_int_t lws_init_monitor(ngx_shm_zone_t *zone, void *data);
static ngx_int_t lws_monitor_handler(ngx_http_request_t *r);
static ngx_int_t lws_monitor_content(ngx_http_request_t *r);
static ngx_int_t lws_monitor_action(ngx_http_request_t *r);
static void lws_monitor_body(ngx_http_request_t *r);
static ngx_int_t lws_monitor_pair(ngx_http_request_t *r, ngx_str_t *key, ngx_str_t *value);


char *lws_monitor(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
	ngx_str_t                  name;
	lws_loc_conf_t            *llcf;
	lws_main_conf_t           *lmcf;
	ngx_http_core_loc_conf_t  *clcf;

	/* set monitor */
	llcf = conf;
	if (llcf->monitor) {
		return "is duplicate";
	}
	if (llcf->main) {
		return "is exclusive with lws directive";
	}
	llcf->monitor = 1;

	/* add shared memory zone */
	lmcf = ngx_http_conf_get_module_main_conf(cf, lws);
	if (!lmcf->monitor_shm) {
		ngx_str_set(&name, "lws_monitor");
		lmcf->monitor_shm = ngx_shared_memory_add(cf, &name, LWS_MONITOR_SIZE, &lws);
		if (!lmcf->monitor_shm) {
			return NGX_CONF_ERROR;
		}
		lmcf->monitor_shm->noreuse = 1;
		lmcf->monitor_shm->data = lmcf;
		lmcf->monitor_shm->init = lws_init_monitor;
	}

	/* install handler */
	clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
	clcf->handler = lws_monitor_handler;

	return NGX_CONF_OK;
}

static ngx_int_t lws_init_monitor (ngx_shm_zone_t *zone, void *data) {
	lws_main_conf_t  *lmcf;

	lmcf = zone->data;
	lmcf->monitor_pool = (ngx_slab_pool_t *)zone->shm.addr;
	lmcf->monitor = ngx_slab_calloc(lmcf->monitor_pool, sizeof(lws_monitor_t));
	if (!lmcf->monitor) {
		return NGX_ERROR;
	}
	lmcf->monitor->functions_alloc = 32;
	lmcf->monitor->functions = ngx_slab_alloc(lmcf->monitor_pool,
			lmcf->monitor->functions_alloc * sizeof(lws_function_t));
	if (!lmcf->monitor->functions) {
		return NGX_ERROR;
	}
	return NGX_OK;
}

static ngx_int_t lws_monitor_handler (ngx_http_request_t *r) {
	switch (r->method) {
	case NGX_HTTP_GET:
		return lws_monitor_content(r);

	case NGX_HTTP_POST:
		return lws_monitor_action(r);

	default:
		return NGX_HTTP_NOT_ALLOWED;
	}
}

static ngx_int_t lws_monitor_content (ngx_http_request_t *r) {
	size_t            i, len;
	ngx_buf_t        *b;
	ngx_int_t         rc;
	ngx_chain_t      *out;
	lws_function_t   *f;
	lws_main_conf_t  *lmcf;
	ngx_table_elt_t  *h;

	/* allocate buffer */
	out = ngx_alloc_chain_link(r->pool);
	b = ngx_calloc_buf(r->pool);
	if (!out || !b) {
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	/* build JSON */
	lmcf = ngx_http_cycle_get_module_main_conf(ngx_cycle, lws);
	ngx_shmtx_lock(&lmcf->monitor_pool->mutex);
	len = sizeof("{\n") - 1;
	len += sizeof("\t\"states_n\": ,\n") - 1  + 20;
	len += sizeof("\t\"requests_n\": ,\n") - 1  + 20;
	len += sizeof("\t\"memory_used\": ,\n") - 1  + 20;
	len += sizeof("\t\"request_count\": ,\n") - 1  + 20;
	len += sizeof("\t\"out_of_memory\": ,\n") - 1  + 1;
	len += sizeof("\t\"profiler\": ,\n") - 1  + 1;
	len += sizeof("\t\"functions\": [\n") - 1;
	for (i = 0; i < lmcf->monitor->functions_n; i++) {
		f = &lmcf->monitor->functions[i];
		len += sizeof("\t\t[\"\", , , , , , ],\n") - 1 + 6 * 20;
		len += ngx_escape_json(NULL, f->key.data, f->key.len);
	}
	len += sizeof("\t]\n}") - 1;
	b->start = ngx_palloc(r->pool, len);
	if (!b->start) {
		ngx_shmtx_unlock(&lmcf->monitor_pool->mutex);
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}
	b->pos = b->start;
	b->end = b->start + len;
	b->last = b->pos;
	b->last = ngx_sprintf(b->last, "{\n"
			"\t\"states_n\": %i,\n"
			"\t\"requests_n\": %i,\n"
			"\t\"memory_used\": %i,\n"
			"\t\"request_count\": %i,\n"
			"\t\"out_of_memory\": %i,\n"
			"\t\"profiler\": %i,\n",
			(ngx_int_t)lmcf->monitor->states_n,
			(ngx_int_t)lmcf->monitor->requests_n,
			(ngx_int_t)lmcf->monitor->memory_used,
			(ngx_int_t)lmcf->monitor->request_count,
			(ngx_int_t)lmcf->monitor->out_of_memory,
			(ngx_int_t)lmcf->monitor->profiler);
	if (lmcf->monitor->functions_n == 0) {
		b->last = lws_cpylit(b->last, "\t\"functions\": []\n");
	} else {
		b->last = lws_cpylit(b->last, "\t\"functions\": [\n");
		for (i = 0; i < lmcf->monitor->functions_n; i++) {
			f = &lmcf->monitor->functions[i];
			b->last = lws_cpylit(b->last, "\t\t[\"");
			b->last = (u_char *)ngx_escape_json(b->last, f->key.data, f->key.len);
			b->last = ngx_sprintf(b->last, "\", %i, %i, %i, %i, %i, %i]",
					(ngx_int_t)f->calls,
					(ngx_int_t)f->time_self.tv_sec,
					(ngx_int_t)f->time_self.tv_nsec,
					(ngx_int_t)f->time_total.tv_sec,
					(ngx_int_t)f->time_total.tv_nsec,
					(ngx_int_t)f->memory);
			if (i < lmcf->monitor->functions_n - 1) {
				b->last = lws_cpylit(b->last, ",\n");
			} else {
				b->last = lws_cpylit(b->last, "\n");
			}
		}
		b->last = lws_cpylit(b->last, "\t]\n");
	}
	b->last = lws_cpylit(b->last, "}");
	ngx_shmtx_unlock(&lmcf->monitor_pool->mutex);

	/* send headers */
	r->headers_out.status = NGX_HTTP_OK;
	ngx_str_set(&r->headers_out.content_type, "application/json");
	r->headers_out.content_type_len = r->headers_out.content_type.len;
	h = ngx_list_push(&r->headers_out.headers);
	if (!h) {
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}
	ngx_str_set(&h->key, "Cache-Control");
	ngx_str_set(&h->value, "private, no-store");
	h->hash = 1;
	r->headers_out.content_length_n = b->last - b->pos;
	rc = ngx_http_send_header(r);
	if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
		return rc;
	}

	/* send body */
	b->temporary = 1;
	b->last_buf = (r == r->main) ? 1 : 0;
	b->last_in_chain = 1;
	out->buf = b;
	out->next = NULL;
	rc = ngx_http_output_filter(r, out);
	ngx_free_chain(r->pool, out);
	return rc;
}

static ngx_int_t lws_monitor_action (ngx_http_request_t *r) {
	ngx_int_t  rc;

	/* read request body */
	rc = ngx_http_read_client_request_body(r, lws_monitor_body);
	if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
		return rc;
	}
	return NGX_DONE;
}

static void lws_monitor_body (ngx_http_request_t *r) {
	int           state;
	u_char       *pos, *start, *last;
	ngx_int_t     rc;
	ngx_str_t     body, key, value;
	ngx_chain_t  *cl;

	/* check request body */
	if (!r->request_body) {
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
		return;
	}
	if (!r->request_body->bufs) {
		ngx_http_finalize_request(r, NGX_HTTP_BAD_REQUEST);
		return;
	}

	/* read request body */
	body.len = 0;
	for (cl = r->request_body->bufs; cl; cl = cl->next) {
		body.len += ngx_buf_size(cl->buf);
	}
	body.data = ngx_palloc(r->pool, body.len);
	if (!body.data) {
		ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
		return;
	}
	pos = body.data;
	for (cl = r->request_body->bufs; cl; cl = cl->next) {
		pos = ngx_cpymem(pos, cl->buf->pos, cl->buf->last - cl->buf->pos);
	}

	/* process request body */
	state = 0;
	pos = body.data;
	last = body.data + body.len;
	value.data = NULL;
	while (1) {
		/* find key (state == 0) or value (state == 1) */
		start = pos;
		while (pos < last && *pos != '=' && *pos != '&') {
			pos++;
		}
		if (state == 0) {
			key.data = start;
			key.len = pos - start;
			if (*pos == '=') {
				state = 1;
			} else {
				value.data = pos;
				value.len = 0;
			}
		} else {
			value.data = start;
			value.len = pos - start;
			state = 0;
		}

		/* process pair */
		if (value.data) {
			rc = lws_monitor_pair(r, &key, &value);
			if (rc != NGX_OK) {
				ngx_http_finalize_request(r, rc);
				return;
			}
			value.data = NULL;
		}

		/* next pair */
		if (pos == last) {
			break;
		}
		pos++;
	}
	ngx_http_finalize_request(r, lws_monitor_content(r));
}

static ngx_int_t lws_monitor_pair (ngx_http_request_t * r, ngx_str_t *key, ngx_str_t *value) {
	lws_main_conf_t  *lmcf;

	lmcf = ngx_http_get_module_main_conf(r, lws);
	if (key->len == 8 && ngx_strncmp(key->data, "profiler", 8) == 0) {
		if (value->len != 1) {
			return NGX_HTTP_BAD_REQUEST;
		}
		switch (*value->data) {
		case '0':
			if (!ngx_atomic_cmp_set(&lmcf->monitor->profiler, 1, 0)) {
				return NGX_HTTP_CONFLICT;
			}
			break;

		case '1':
			if (!ngx_atomic_cmp_set(&lmcf->monitor->profiler, 0, 1)) {
				return NGX_HTTP_CONFLICT;
			}
			break;

		default:
			return NGX_HTTP_BAD_REQUEST;
		}
	}

	return NGX_OK;
}
