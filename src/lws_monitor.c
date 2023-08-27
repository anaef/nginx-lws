/*
 * LWS monitor
 *
 * Copyright (C) 2023 Andre Naef
 */


#include <lws_module.h>


static ngx_int_t lws_init_monitor(ngx_shm_zone_t *zone, void *data);


char *lws_monitor(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
	ngx_str_t         name;
	lws_loc_conf_t   *llcf;
	lws_main_conf_t  *lmcf;

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
