# LWS Installation

To install lws-nginx, you may find it helpful to first review this
[general blog post](https://www.nginx.com/blog/compiling-dynamic-modules-nginx-plus/)
on compiling dynamic modules for NGINX if you are not familiar with the process.

In brief:

1. Determine your NGINX version by typing `nginx -v`.
2. Download and unpack the [NGINX source code](https://nginx.org/download/) with the identified
version.
3. Clone this repository.
4. In the NGINX directory, run `./configure --with-compat --add-dynamic-module=../lws-nginx`, and
then `make modules`. (If debug logging is required, add `--with-debug`.)
5. Copy the `objs/lws.so` shared library into the NGINX modules folder, e.g.,
`/usr/lib/nginx/modules`.
6. Add the directive `load_module modules/lws.so;` to the NGINX main configuration. (Alternatively,
you can copy the provided `etc/lws.conf` file into a folder where NGINX is configured to read
configuration directives from, such as `/etc/nginx/modules-available` with a corresponding symlink
in `/etc/nginx/modules-enabled`.)
