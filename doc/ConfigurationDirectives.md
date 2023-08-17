# LWS Configuration Directives

This document describes the NGINX configuration directives provided by lws-nginx.


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


## lws_memory_limit *memory_limit*

Context: server, location

Sets a memory limit in bytes for each Lua state. If a Lua state exceeds the limit, a Lua memory
error is generated. A value of `0` disables this check. The default value for *memory_limit* is
`0`.  When setting *memory_limit*, you can use the `k` and `m` suffixes to set kilobytes or
megabytes, respectively.


## lws_lifecycles *lifecycles*

Context: server, location

Sets a limit on the number of lifecycles per Lua state. If a Lua state has serviced the set
number of requests, it is closed. A value of `0` disables this logic. The default value for
*lifecycles* is `0`. Setting the value to `1` closes Lua states after each request. This can be
helpful during local development to pick up code changes.


## lws_gc *gc*

Context: server, location

Sets a number of requests for periodic garbage collection on Lua states. If a Lua state has
serviced the set number of requests, a full garbage collection cycle is performed. A vlaue of
`0` disables this logic. The default value for *gc* is `0`. Setting the value to `1` performs
a full garbage collection cycle after each request.


## lws_thread_pool *thread_pool_name*

Context: http

Sets the name of the thread pool used by LWS for serving requests asynchronously. The default
value of *thread_pool_name* is `default`.


## lws_stat_cache *cap*, *timeout*

Context: http

Sets the parameters of the stat cache. The stat cache maintains file existence information for
speeding up request processing. It maintains up to `cap` entries for a duration of `timeout`
seconds using a least recently used (LRU) algorithm. The default values for *cap* and *timeout* are
`1024` and `30`. You can use the `k` and `m` suffixes with *cap* to set multiples of 1024 or
1024², respectively, and you can use the `s`, `m`, `h`, `d`, `w`, `M`, and `y` suffixes
with *timeout* to set seconds, minutes, hours, days, weeks, months, or years, respectively.
