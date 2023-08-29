/*
 * LWS monitor
 *
 * Copyright (C) 2023 Andre Naef
 */


#include <lws_monitor.h>
#include <lws_module.h>


static ngx_int_t lws_init_monitor(ngx_shm_zone_t *zone, void *data);
static ngx_int_t lws_monitor_handler(ngx_http_request_t *r);


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
		lmcf->monitor_shm = ngx_shared_memory_add(cf, &name, 64 * 1024, &lws);
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
	lmcf->monitor = ngx_slab_alloc(lmcf->monitor_pool, sizeof(lws_monitor_t));
	if (!lmcf->monitor) {
		return NGX_ERROR;
	}
	ngx_memzero(lmcf->monitor, sizeof(lws_monitor_t));
	return NGX_OK;
}

static ngx_int_t lws_monitor_handler (ngx_http_request_t *r) {
	u_char            output[1024];
	ngx_buf_t        *b;
	ngx_chain_t      *out;
	ngx_table_elt_t  *h;
	ngx_int_t         rc;
	lws_main_conf_t  *lmcf;

	/* allocate buffer */
	out = ngx_alloc_chain_link(r->pool);
	b = ngx_calloc_buf(r->pool);
	if (!out || !b) {
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}
	b->start = output;
	b->pos = b->start;
	b->end = b->start + sizeof(output);
	b->last = b->pos;

	/* build JSON */
	lmcf = ngx_http_get_module_main_conf(r, lws);
	b->last = ngx_slprintf(b->last, b->end, "{\n\tstates_n: %i,\n\trequests_n: %i,\n"
			"\tmemory_used: %i,\n\trequest_count: %i\n}",
			(ngx_int_t)lmcf->monitor->states_n,
			(ngx_int_t)lmcf->monitor->requests_n,
			(ngx_int_t)lmcf->monitor->memory_used,
			(ngx_int_t)lmcf->monitor->request_count);

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
