# LWS Installation

To install LWS, you may find it helpful to first review this
[general blog post](https://www.nginx.com/blog/compiling-dynamic-modules-nginx-plus/)
on compiling dynamic modules for NGINX if you are not familiar with the process.

In brief:

1. Clone this repository, i.e., nginx-lws.
1. If you require a Lua version other than 5.4, edit the `config` file in the nginx-lws directory,
and change the `lws_lua` variable accordingly.
1. Determine your NGINX version by typing `nginx -v`.
1. Download the [NGINX source code](https://nginx.org/download/) with the identified version,
and unpack it into a sibling folder of the nginx-lws directory.
1. In the NGINX directory, run
`./configure --with-compat --with-threads --add-dynamic-module=../nginx-lws`. If debug logging is
required, add `--with-debug`.
1. Run `make modules`.
1. Copy the `objs/lws_module.so` shared library into the NGINX modules folder, e.g.,
`/usr/lib/nginx/modules`. The particularities depend on your system and NGINX installation.
1. Add the directive `load_module modules/lws_module.so;` to the NGINX main configuration.,
Alternatively, you can copy the provided `etc/lws.conf` file into a folder where NGINX is
configured to read configuration directives from, such as `/etc/nginx/modules-available` with a
corresponding symlink in `/etc/nginx/modules-enabled`. Again, the particularities depend on
your system and NGINX installation.
