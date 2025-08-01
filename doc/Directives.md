# LWS Directives

This document describes the NGINX directives provided by LWS.


## HTTP Main Configuration

The following directives are set in the HTTP main configuration.


### lws_thread_pool *thread_pool_name*

Context: http

Sets the name of the thread pool used by LWS for serving requests asynchronously. The default
value of *thread_pool_name* is `default`.

> [!IMPORTANT]
> If the thread pool name is different from `default`, the named thread pool must be defined with
> the NGINX `thread_pool` directive in the main context of the NGINX configuration.


### lws_stat_cache *cap* *timeout*

Context: http

Sets the parameters of the stat cache. The stat cache maintains file existence information for
speeding up request processing. It maintains up to `cap` entries for a duration of `timeout`
seconds using a least recently used (LRU) algorithm. The default values for *cap* and *timeout*
are `1024` and `30`. You can use the `k` and `m` suffixes with *cap* to set multiples of 1024 or
1024², respectively, and you can use the `s`, `m`, `h`, `d`, `w`, `M`, and `y` suffixes
with *timeout* to set seconds, minutes, hours, days, weeks, months, or years, respectively.


## HTTP Location Configuration

The following directives are set in the HTTP location configuration. Where it is meaningful, they
can also be set in the HTTP server configuration, providing a default value for the location
configurations of the server.


### lws *main* [*path_info*]

Context: location

Enables the LWS handler for the main Lua chunk with filename *main*. The optional *path_info*
is passed in the request context. Both *main* and *path_info* can contain variables. This is
useful for including captures from the location path as illustrated by the following example:

```nginx
server {
	...
        location ~ /services/(\w+)(/.*)? {
                lws /var/www/lws-examples/services/$1.lua $2;
		...
	}
}
```

For more information, please see the [request processing](RequestProcessing.md) documentation.

This directive is exclusive with the `lws_monitor` directive.


### lws_init *init*

Context: server, location

Sets the filename of an init Lua chunk. This chunk initializes Lua states at the location. Please
see the [request processing](RequestProcessing.md) documentation for more information.


### lws_pre *pre*

Context: server, location

Sets the filename of a pre Lua chunk. This chunk is run before the main chunks at the location.
Please see the [request processing](RequestProcessing.md) documentation for more information.


### lws_post *post*

Context: server, location

Sets the filename of a post Lua chunk. This chunk is run after the main chunks at the location.
Please see the [request processing](RequestProcessing.md) documentation for more information.


### lws_path *path*

Context: server, location


Sets the Lua path. If the first character of *path* is `+`, *path* is appended to the default Lua
path.


### lws_cpath *cpath*

Context: server, location

Sets the Lua C path. If the first character of *cpath* is `+`, *cpath* is appended to the default
Lua C path.


### lws_max_states *max_states* [*max_requests*]

Context: server, location

Sets the maximum number of Lua states per worker process and location. If more concurrent requests
arrive than *max_states*, the requests are queued until a Lua state becomes available. A value of
`0`, the default, turns off this logic, making the number of Lua states unrestricted. The queue
accepts up to *max_requests* requests. A 503 Service Unavailable status is returned if the queue
overflows. A value of `0`, the default, turns off this logic, making the queue unrestricted. You
can use the `k` and `m` suffixes with *max_states* and *max_requests* to set multiples of 1024 or
1024², respectively.


### lws_max_memory *max_memory*

Context: server, location

Sets the maximum memory of a Lua state. A Lua memory error is generated if the memory allocated by
a Lua state would exceed *max_memory* bytes. A value of `0`, the default, turns off this logic,
making the amount of memory unrestricted. You can use the `k` and `m` suffixes with *max_memory*
to set kilobytes or megabytes, respectively.

> [!NOTE]
> The term *memory* in the context of LWS and Lua states generally refers to the memory allocated
> by the Lua states per se, i.e., through their memory allocators. This memory does *not* include
> memory allocated outside of Lua states, such as in Lua C libraries or NGINX.


### lws_gc *gc*

Context: server, location

Sets the memory threshold of a Lua state that triggers an explicit garbage collection cycle. If
the memory allocated by a Lua state exceeds *gc* bytes when a request is finalized, an explicit,
full garbage collection cycle is performed. A value of `0`, the default, turns off this logic.
Setting the value to `1` performs a full garbage collection cycle after each request. You can use
the `k` and `m` suffixes with *gc* to set kilobytes or megabytes, respectively.

> [!NOTE]
> Please see the note on the term *memory* above.


### lws_max_requests *max_requests*

Context: server, location

Sets the maximum number of requests in the lifecycle of a Lua state. If a Lua state has serviced
the specified number of requests, it is closed. A value of `0`, the default, turns off this logic.

> [!NOTE]
> Setting the value to `1` closes Lua states after each request. This can be helpful during local
> development to pick up code changes.


### lws_max_time *max_time*

Context: server, location

Sets the maximum lifecycle time of a Lua state. If a Lua state has been alive for a duration of
`max_time` seconds, it is closed. A value of `0`, the default, turns off this logic. You can
use the `ms`, `s`, `m`, `h`, `d`, `w`, and `M` suffixes with *max_time* to set milliseconds,
seconds, minutes, hours, days, weeks, or months, respectively.


### lws_timeout *timeout*

Context: server, location

Sets the maximum idle time of a Lua state. If a Lua state has been inactive for a duration of
`timeout` seconds, it is closed. A value of `0`, the default, turns off this logic. You can
use the `ms`, `s`, `m`, `h`, `d`, `w`, and `M` suffixes with *timeout* to set milliseconds,
seconds, minutes, hours, days, weeks, or months, respectively.


### lws_variable *variable*

Context: server, location

Resolves the value of an NGINX variable. The value of the variable with name *variable* can then
be queried through the `lws.getvariable` [library function](Library.md). This directive can be
used repeatedly.


### lws_error_response *error_response* [*attribute*]

Context: server, location

Sets the content of error responses. Error responses are sent if a Lua error is generated
or a pre or main chunk returns a positive integer result. In the first case, the error response
is 500 Internal Server Error; in the second case, the error response is for the corresponding
HTTP status code. The *error_response* value controls the response's content type and can take
the values `json` or `html`. The default value for *error_response* is `json`. The optional
*attribute* value controls whether diagnostic information is included for Lua errors and can take
the value `diagnostic`. Diagnostic information includes the error message, file names, line
numbers, function identifiers, and a stack traceback. By default, and if the attribute is omitted,
diagnostic information is not included.

> [!CAUTION]
> Diagnostic information can be helpful during development. Enabling diagnostic information on
> non-development systems is however not recommended, as such information can be exploited by
> attackers.


### lws_streaming *streaming*

Context: server, location

Controls the availability of HTTP response streaming. The *streaming* value can take the values
`on` or `off`. The default value for *streaming* is `off`.


### lws_monitor

Context: location

Enables the [LWS monitor](Monitor.md) at the location. The LWS monitor is a web API that provides
read-write access to central LWS characteristics. This directive is exclusive with the `lws`
directive.

> [!CAUTION]
> The LWS monitor should *not* be enabled at locations that are publicly accessible. Enabling
> the monitor at a location without appropriate access controls is a security risk.
