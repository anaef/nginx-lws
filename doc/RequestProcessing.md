## LWS Request Processing

This document describes the general HTTP request processing logic of LWS.

Request processing involves up to four Lua chunks: an init chunk, a pre chunk, a main chunk, and a
post chunk.


## Init Lua Chunk

The init Lua chunk is run only once per Lua state. It provides the opportunity to initialize a Lua
state and set up shared resources for requests, such as database connections.

The init Lua chunk runs with the global environment of the Lua state.


## Pre, Main, and Post Lua Chunks

The pre, main, and post Lua chunks are run sequentially for each request. The optional pre and post
chunks provide the opportunity to perform common tasks for the main chunks at the location.

The pre, main, and post Lua chunks run with a request environment that indexes the global
environment for undefined values.


## Request Environment

The request environment provides the values `request` and `response`. These table values
manage information pertinent to the HTTP request.

### `request` Value

| Field | Description |
| --- | --- |
| method | HTTP request method |
| uri | HTTP request URI |
| path_info | Path info, as defined with the `lws` directive |
| args | HTTP request query arguments |
| headers | HTTP request headers (case-insensitive keys, read-only) |
| body | HTTP request body (Lua file handle interface, read-only) |


### `response` Value

| Field | Description |
| --- | --- |
| status | HTTP response status (defaults to 200) |
| headers | HTTP response headers (case-insensitive keys) |
| body | HTTP response body (Lua file handle interface, write-only) |


## Chunk Result

A chunk must return no value, `nil`, or an integer as its result. No value, nil, and `0` indicate
success. A negative integer indicates failure and generates a Lua error. Results with a bad type are
processed as `-1`.

For the main chunk only, a positive integer between 100 and 599 inclusive causes the web server to
render a default response page for the corresponding HTTP status code. 

> [!NOTE]
> Most main chunks produce a response body directly and must set `response.status` rather than
> returning an HTTP status code.


## Processing Sequence

The Lua chunks are run in the order init (as required), pre, main, and post. If any chunk
generates a Lua error, be it directly or indirectly through its result, the processing is
aborted.


## Lifecycle of Lua States

Lua states are kept open to handle subsequent requests when a request completes.

> [!WARNING]
> Developers must be careful not to leak information from request to request, such as through the
> global environment or the Lua registry. Any request-specific state should be constrained to
> the request environment, and local variables.

Lua states read the Lua chunks from the file systems only once for performance.

You can control the closing of Lua states with the `lws_lifecycles` directive.
