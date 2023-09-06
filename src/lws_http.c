/*
 * LWS HTTP
 *
 * Copyright (C) 2023 Andre Naef
 */


#include <lws_http.h>
#include <ngx_http.h>


#define LWS_HTTP_STATUS_N (3 + 7 + 8 + 24 + 7)


lws_http_status_t lws_http_status[LWS_HTTP_STATUS_N] = {
	/* 1xx [n=3] */
	{ NGX_HTTP_CONTINUE, ngx_string("CONTINUE"), ngx_string("Continue") },
	{ NGX_HTTP_SWITCHING_PROTOCOLS, ngx_string("SWITCHING_PROTOCOLS"),
			ngx_string("Switching Protocols") },
	{ NGX_HTTP_PROCESSING, ngx_string("PROCESSING"), ngx_string("Processing") },

	/* 2xx [n=7] */
	{ NGX_HTTP_OK, ngx_string("OK"), ngx_string("OK") },
	{ NGX_HTTP_CREATED, ngx_string("CREATED"), ngx_string("Created") },
	{ NGX_HTTP_ACCEPTED, ngx_string("ACCEPTED"), ngx_string("Accepted") },
	{ 203, ngx_string("NON_AUTHORITATIVE_INFORMATION"),
			ngx_string("Non-Authoritative Information") },
	{ NGX_HTTP_NO_CONTENT, ngx_string("NO_CONTENT"), ngx_string("No Content") },
	{ 205, ngx_string("RESET_CONTENT"), ngx_string("Reset Content") },
	{ NGX_HTTP_PARTIAL_CONTENT, ngx_string("PARTIAL_CONTENT"), ngx_string("Partial Content") },

	/* 3xx [n=8] */
	{ NGX_HTTP_SPECIAL_RESPONSE, ngx_string("MULTIPLE_CHOICES"),
			 ngx_string("Multiple Choices") },
	{ NGX_HTTP_MOVED_PERMANENTLY, ngx_string("MOVED_PERMANENTLY"),
			ngx_string("Moved Permanently") },
	{ NGX_HTTP_MOVED_TEMPORARILY, ngx_string("FOUND"), ngx_string("Found") },
	{ NGX_HTTP_SEE_OTHER, ngx_string("SEE_OTHER"), ngx_string("See Other") },
	{ NGX_HTTP_NOT_MODIFIED, ngx_string("NOT_MODIFIED"), ngx_string("Not Modified") },
	{ 305, ngx_string("USE_PROXY"), ngx_string("Use Proxy") },
	{ NGX_HTTP_TEMPORARY_REDIRECT, ngx_string("TEMPORARY_REDIRECT"),
			ngx_string("Temporary Redirect") },
	{ NGX_HTTP_PERMANENT_REDIRECT, ngx_string("PERMANENT_REDIRECT"),
			ngx_string("Permanent Redirect") },

	/* 4xx [n=24] */
	{ NGX_HTTP_BAD_REQUEST, ngx_string("BAD_REQUEST"), ngx_string("Bad Request") },
	{ NGX_HTTP_UNAUTHORIZED, ngx_string("UNAUTHORIZED"), ngx_string("Unauthorized") },
	{ 402, ngx_string("PAYMENT_REQUIRED"), ngx_string("Payment Required") },
	{ NGX_HTTP_FORBIDDEN, ngx_string("FORBIDDEN"), ngx_string("Forbidden") },
	{ NGX_HTTP_NOT_FOUND, ngx_string("NOT_FOUND"), ngx_string("Not Found") },
	{ NGX_HTTP_NOT_ALLOWED, ngx_string("METHOD_NOT_ALLOWED"),
			ngx_string("Method Not Allowed") },
	{ 406, ngx_string("NOT_ACCEPTABLE"), ngx_string("Not Acceptable") },
	{ 407, ngx_string("PROXY_AUTHENTICATION_REQUIRED"),
			ngx_string("Proxy Authentication Required") },
	{ NGX_HTTP_REQUEST_TIME_OUT, ngx_string("REQUEST_TIMEOUT"), ngx_string("Request Timeout") },
	{ NGX_HTTP_CONFLICT, ngx_string("CONFLICT"), ngx_string("Conflict") },
	{ 410, ngx_string("GONE"), ngx_string("Gone") },
	{ NGX_HTTP_LENGTH_REQUIRED, ngx_string("LENGTH_REQUIRED"), ngx_string("Length Required") },
	{ NGX_HTTP_PRECONDITION_FAILED, ngx_string("PRECONDITION_FAILED"),
			ngx_string("Precondition Failed") },
	{ NGX_HTTP_REQUEST_ENTITY_TOO_LARGE, ngx_string("CONTENT_TOO_LARGE"),
			ngx_string("Content Too Large") },
	{ NGX_HTTP_REQUEST_ENTITY_TOO_LARGE, ngx_string("REQUEST_ENTITY_TOO_LARGE"),
			ngx_string("Request Entity Too Large") },  /* legacy */
	{ NGX_HTTP_REQUEST_URI_TOO_LARGE, ngx_string("URI_TOO_LONG"), ngx_string("URI Too Long") },
	{ NGX_HTTP_REQUEST_URI_TOO_LARGE, ngx_string("REQUEST_URI_TOO_LARGE"),
			ngx_string("Request URI Too Large") },  /* legacy */
	{ NGX_HTTP_UNSUPPORTED_MEDIA_TYPE, ngx_string("UNSUPPORTED_MEDIA_TYPE"),
			ngx_string("Unsupported Media Type") },
	{ NGX_HTTP_RANGE_NOT_SATISFIABLE, ngx_string("RANGE_NOT_SATISFIABLE"),
			ngx_string("Range Not Satisfiable") },
	{ 417, ngx_string("EXPECTATION_FAILED"), ngx_string("Expectation Failed") },
	{ NGX_HTTP_MISDIRECTED_REQUEST, ngx_string("MISDIRECTED_REQUEST"),
			ngx_string("Misdirected Request") },
	{ 422, ngx_string("UNPROCESSABLE_CONTENT"), ngx_string("Unprocessable Content") },
	{ 426, ngx_string("UPGRADE_REQUIRED"), ngx_string("Upgrade Required") },
	{ NGX_HTTP_TOO_MANY_REQUESTS, ngx_string("TOO_MANY_REQUESTS"),
			ngx_string("Too Many Requests") },

	/* 5xx [n=7] */
	{ NGX_HTTP_INTERNAL_SERVER_ERROR, ngx_string("INTERNAL_SERVER_ERROR"),
			ngx_string("Internal Server Error") },
	{ NGX_HTTP_NOT_IMPLEMENTED, ngx_string("NOT_IMPLEMENTED"),
			ngx_string("Not Implemented") },
	{ NGX_HTTP_BAD_GATEWAY, ngx_string("BAD_GATEWAY"),
			ngx_string("Bad Gateway") },
	{ NGX_HTTP_SERVICE_UNAVAILABLE, ngx_string("SERVICE_UNAVAILABLE"),
			ngx_string("Service Unavailable") },
	{ NGX_HTTP_GATEWAY_TIME_OUT, ngx_string("GATEWAY_TIMEOUT"),
			ngx_string("Gateway Timeout") },
	{ NGX_HTTP_VERSION_NOT_SUPPORTED, ngx_string("HTTP_VERSION_NOT_SUPPORTED"),
			ngx_string("HTTP Version Not Supported") },
	{ NGX_HTTP_INSUFFICIENT_STORAGE, ngx_string("INSUFFICIENT_STORAGE"),
			ngx_string("Insufficient Storage") },
};
const int lws_http_status_n = LWS_HTTP_STATUS_N;


lws_http_status_t *lws_find_http_status (int code) {
	int  lower, upper, mid;

	lower = 0;
	upper = lws_http_status_n;
	while (lower <= upper) {
		mid = (lower + upper) / 2;
		if (lws_http_status[mid].code < code) {
			lower = mid + 1;
		} else {
			upper = mid - 1;
		}
	}
	return lower < lws_http_status_n && lws_http_status[lower].code == code ?
			&lws_http_status[lower] : NULL;
}
