# LWS Directives

This document describes the NGINX directives provided by lws-nginx.


## lws *main* [, *path_info*]

Context: location

Enables the LWS handler for the main Lua chunk with filename *main*. The optional *path_info*
is passed in the request context. Both *main* and *path_info* can contain variables. This is useful
for including captures from the location path.

Example:
```nginx
server {
	...
        location ~ /services/(\w+)(/.*)? {
                lws /var/www/lws-examples/services/$1.lua $2;
		...
	}
}
```


## lws_init *init*

Context: server, location

Sets the filename of an init Lua chunk. This chunk initializes Lua states at the location.


## lws_pre *pre*

Context: server, location

Sets the filename of a pre Lua chunk. This chunk is run before each request at the location.


## lws_post *post*

Context: server, location

Sets the filename of a post Lua chunk. This chunk is run after each request at the location.


## lws_path *path*

Context: server, location


Sets the Lua path. If the first character of *path* is `+`, *path* is appended to the default Lua
path.


## lws_cpath *cpath*

Context: server, location

Sets the Lua C path. If the first character of *cpath* is `+`, *cpath* is appended to the default
Lua C path.


## lws_max_memory *max_memory*

Context: server, location

Sets the maximum memory of a Lua state. If the memory allocated by a Lua state exceeds
*max_memory* bytes, a Lua memory error is generated. A value of `0` disables this
logic. The default value for *max_memory* is `0`. You can use the `k` and `m` suffixes with
*max_memory* to set kilobytes or megabytes, respectively.


## lws_max_requests *max_requests*

Context: server, location

Sets the maximum number of requests in the lifecycle of a Lua state. If a Lua state has serviced
the set number of requests, it is closed. A value of `0` disables this logic. The default value
for *max_requests* is `0`. Setting the value to `1` closes Lua states after each request. This
can be helpful during local development to pick up code changes.


## lws_max_time *max_time*

Context: server, location

Sets the maximum lifecycle time of a Lua state. If a Lua state has been alive for a duration
of `max_time` milliseconds, it is closed. A value of `0` disables this logic. The default value
for *max_time* is `0`. You can use the `ms`, `s`, `m`, `h`, `d`, `w`, and `M` suffixes with
*max_time* to set milliseconds, seconds, minutes, hours, days, weeks, or months, respectively.


## lws_timeout *timeout*

Context: server, location

Sets the maximum idle time of a Lua state. If a Lua state has been idle for a duration of
`timeout` milliseconds, it is closed. A value of `0` disables this logic. The default value
for *timeout* is `0`. You can use the `ms`, `s`, `m`, `h`, `d`, `w`, and `M` suffixes with
*timeout* to set milliseconds, seconds, minutes, hours, days, weeks, or months, respectively.


## lws_gc *gc*

Context: server, location

Sets the memory threshold of a Lua state which triggers an explicit garbage collection cycle. If
the memory allocated by a Lua exceeds *gc* bytes when a request completes, an explicit, full
garbage collection cycle is performed. A value of `0` disables this logic. The default value for
*gc* is `0`. Setting the value to `1` performs a full garbage collection cycle after each request.
You can use the `k` and `m` suffixes with *gc* to set kilobytes or megabytes, respectively.


## lws_error_response *error_response* [, *diagnostic*]

Context: server, location

Sets the content of error responses. Error responses are sent if a Lua error is generated, or if a
pre, main, or post chunk returns a positive integer result. In the first case, the error response
is 500 Internal Server Error; in the second case, the error response is for the corresponding HTTP
status code. The *error_response* value controls the content type of the response and must be one
of `json` or `html`. The default value for *error_response* is `json`. The optional *diagnostic*
value controls whether diagnostic information is included for Lua errors and must be one of `off`
or `on`. The default value for *diagnostic* is `off`. Diagnostic information includes the error
message, file names, line numbers and a stack traceback.


## lws_thread_pool *thread_pool_name*

Context: http

Sets the name of the thread pool used by LWS for serving requests asynchronously. The default
value of *thread_pool_name* is `default`.


## lws_stat_cache *cap*, *timeout*

Context: http

Sets the parameters of the stat cache. The stat cache maintains file existence information for
speeding up request processing. It maintains up to `cap` entries for a duration of `timeout`
seconds using a least recently used (LRU) algorithm. The default values for *cap* and *timeout*
are `1024` and `30`. You can use the `k` and `m` suffixes with *cap* to set multiples of 1024 or
1024², respectively, and you can use the `s`, `m`, `h`, `d`, `w`, `M`, and `y` suffixes
with *timeout* to set seconds, minutes, hours, days, weeks, months, or years, respectively.
