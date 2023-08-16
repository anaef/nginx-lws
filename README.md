# Lua Web Services for NGINX

lws-nginx is a module for the [NGINX](https://nginx.org/) server that supports web services written
in Lua, running directly in the server.

lws-nginx supports [PUC-Rio Lua](https://www.lua.org/) and uses a thread pool to run Lua services
asynchronously, thus allowing them to block execution without blocking the NGINX server. These two
aspects were central design motivations for lws-nginx.

While there is much to be said in favor of (and against) non-blocking architectures, the facts
on the ground are that large parts of the Lua ecosystem are *not* non-blocking. Using threads with
relatively low-cost thread context switches seems a reasonable, pragmatic approach.


## Documentation

Please refer to the [doc](doc) folder.


## Status

lws-nginx is a work in progress.


## Limitations

lws-nginx has been tested with Ubuntu 20.04.6 LTS, Lua 5.3,3, and NGINX 1.18.0. Your mileage
may vary with other software versions.

Lua libraries used with lws-nginx must be thread-safe.


## License

lws-nginx is released under the MIT license. See LICENSE for license terms.
