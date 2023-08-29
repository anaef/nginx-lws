/*
 * LWS HTTP
 *
 * Copyright (C) 2023 Andre Naef
 */


#ifndef _LWS_HTTP_INCLUDED
#define _LWS_HTTP_INCLUDED


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct lws_http_status_s {
	const int   code;
	ngx_str_t   key;
	ngx_str_t   message;
} lws_http_status_t;


extern lws_http_status_t lws_http_status[];
extern const int lws_http_status_n;


lws_http_status_t *lws_http_find_status(int code);


#endif /* _LWS_HTTP_INCLUDED */
