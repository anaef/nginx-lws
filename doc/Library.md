# LWS Library

The LWS library provides functions and other values for implementing web services with LWS. The
library is loaded automatically.


## lws.log ([level,] message)

Logs a message in the error log of the server. The argument *message* must be a string value, and
the optional argument *level* can take the values `debug`, `info`, `notice`, `warn`, `err`,
`crit`, `alert`, and `emerg`; it defaults to `err`.


## lws.getvariable (variable)

Returns the value of the NGINX variable with name *variable*. If the variable is not present,
the function returns `nil`. NGINX variables must be declared with the `lws_variable`
[directive](Directives.md).


## lws.redirect (location [, args])

Schedules an internal redirect to *location*. If *location* starts with `@`, it refers to
a named location. Otherwise, *location* is a path, and *args* are optional query parameters. Both
*location* and *args* must be strings. The request becomes internal and can use locations marked
with the `internal` directive. This function can be called from a pre or main chunk, and if
called from a pre chunk, it additionally marks the request as complete and thus causes processing
to proceed directly to the post chunk, skipping the main chunk. A Lua chunk should return after
scheduling an internal redirect.


## lws.setcomplete ()

Marks the request as complete. This function can be called from a pre chunk and causes processing
to proceed directly to the post chunk, skipping the main chunk. A Lua chunk should return after
marking the request as complete.


## lws.setclose ()

Marks the Lua state to be closed. After the request is finalized, the Lua state is closed.


## lws.parseargs (args)

Parses the request query parameters in *args* and returns them as a table. For repeated keys,
only the final value is provided. This function also parses request bodies with a content type of
`application/x-www-form-urlencoded`, i.e., HTML form submissions with the `POST` method.


## lws.status

Represents a table of common HTTP status codes with strings as keys and integers as values.
Indexing this table is strict and generates a Lua error if a key is not present.

| Key | Value |
| --- | --- |
| `CONTINUE` | 100 |
| `SWITCHING_PROTOCOLS` | 101 |
| `PROCESSING` | 102 |
| `OK` | 200 |
| `CREATED` | 201 |
| `ACCEPTED` | 202 |
| `NON_AUTHORITATIVE_INFORMATION` | 203 |
| `NO_CONTENT` | 204 |
| `RESET_CONTENT` | 205 |
| `PARTIAL_CONTENT` | 206 |
| `MULTIPLE_CHOICES` | 300 |
| `MOVED_PERMANENTLY` | 301 |
| `FOUND` | 302 |
| `SEE_OTHER` | 303 |
| `NOT_MODIFIED` | 304 |
| `USE_PROXY` | 305 |
| `TEMPORARY_REDIRECT` | 307 |
| `PERMANENT_REDIRECT` | 308 |
| `BAD_REQUEST` | 400 |
| `UNAUTHORIZED` | 401 |
| `PAYMENT_REQUIRED` | 402 |
| `FORBIDDEN` | 403 |
| `NOT_FOUND` | 404 |
| `METHOD_NOT_ALLOWED` | 405 |
| `NOT_ACCEPTABLE` | 406 |
| `PROXY_AUTHENTICATION_REQUIRED` | 407 |
| `REQUEST_TIMEOUT` | 408 |
| `CONFLICT` | 409 |
| `GONE` | 410 |
| `LENGTH_REQUIRED` | 411 |
| `PRECONDITION_FAILED` | 412 |
| `CONTENT_LOO_LARGE` | 413 |
| `URI_TOO_LONG` | 414 |
| `UNSUPPORTED_MEDIA_TYPE` | 415 |
| `RANGE_NOT_SATISFIABLE` | 416 |
| `EXPECTATION_FAILED` | 417 |
| `MISDIRECTED_REQUEST` | 421 |
| `UNPROCESSABLE_CONTENT` | 422 |
| `UPGRADE_REQUIRED` | 426 |
| `TOO_MANY_REQUESTS` | 429 |
| `INTERNAL_SERVER_ERROR` | 500 |
| `NOT_IMPLEMENTED` | 501 |
| `BAD_GATEWAY` | 502 |
| `SERVICE_UNAVAILABLE` | 503 |
| `GATEWAY_TIMEOUT` | 504 |
| `HTTP_VERSION_NOT_SUPPORTED` | 505 |
| `INSUFFICIENT_STORAGE` | 507 |
