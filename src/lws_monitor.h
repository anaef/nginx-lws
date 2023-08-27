/*
 * LWS monitor
 *
 * Copyright (C) 2023 Andre Naef
 */


#ifndef _LWS_MONITOR_INCLUDED
#define _LWS_MONITOR_INCLUDED


typedef struct lws_monitor_s lws_monitor_t;
struct lws_monitor_s {
	ngx_atomic_t  states_n;
};


char *lws_monitor(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


#endif /* _LWS_MONITOR_INCLUDED */
