# Lua Web Services for NGINX

lws-nginx is a module for the [NGINX](https://nginx.org/) web server that supports web services
written in Lua, running directly in the web server.

lws-nginx supports [PUC-Rio Lua](https://www.lua.org/) and uses a thread pool to run Lua services
asynchronously, thus allowing them to block execution without blocking the NGINX web server. These
two aspects were the central design motivation for lws-nginx.

While there is much to be said in favor of (and against) non-blocking architectures, the facts
on the ground are that large parts of the Lua ecosystem are *not* non-blocking. Using threads with
relatively low-cost thread context switches seems a reasonable, pragmatic approach.


## Installation

To install lws-nginx, you may find it helpful to first review this
[general blog post](https://www.nginx.com/blog/compiling-dynamic-modules-nginx-plus/)
on compiling dynamic modules for NGINX if you are not familiar with the process.

In brief:

1. Determine your NGINX version by typing `nginx -v`.
2. Download and unpack the [NGINX source code](https://nginx.org/download/) with the identified
version.
3. Clone this repository.
4. In the NGINX directory, run `./configure --with-compat --add-dynamic-module=../lws-nginx`, and
then `make modules`.
5. Copy the `objs/lws.so` shared library into the NGINX modules folder, e.g.,
`/usr/lib/nginx/modules`.
6. Add the directive `load_module modules/lws.so;` to the NGINX main configuration. (Alternatively,
you can copy the provided `etc/lws.conf` file into a folder where NGINX is configured to read
configuration directives from, such as `/etc/nginx/modules-available` with a corresponding symlink
in `/etc/nginx/modules-enabled`.)


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
