# Getting Started

LWS comes with a website containing examples. You can find the site in the `/examples` folder of
this repository.


## Examples Website Configuration

The configuration provided with the examples is called `lws-examples.conf` and looks similar to
this:

```nginx
server {
	listen localhost:8080;
	
	error_log /var/log/nginx/error.log;

	location / {
		alias /var/www/lws-examples/static/;
	}

	location ~ /services/(\w+)(/.*)? {
		lws /var/www/lws-examples/services/$1.lua $2;
		lws_init /var/www/lws-examples/handlers/init.lua;
		lws_pre /var/www/lws-examples/handlers/pre.lua;
		lws_path +/var/www/lws-examples/modules/?.lua;
		lws_lifecycles 1;
	}

	location /internal/ {
		alias /var/www/lws-examples/internal/;
		internal;
	}
}
```

This configuration defines a web server with three locations:
- A root location that serves static content from the `static` subfolder.
- A `/services` location that provides the services implemented in Lua.
- A `/internal` location that is only accessible via internal redirects.

The `/services` location uses the central `lws` directive to enable LWS. The directive matches
URIs to files with Lua chunks. Specifically, the URI `/services/{name}` is mapped to the Lua chunk
`/var/www/lws-examples/services/{name}.lua`. The variable `$1` represents the first capture group
of the location. Extra path information following the service name is provided as path info to the
Lua code via the 2nd capture group and the corresponding `$2` variable.

Further used directives include:

- `lws_init`, which refers to a Lua chunk that initializes the state. This is run once per state.
- `lws_pre`, which refers to a Lua chunk that prepares a state for a particular request. This is
run once per request.
- `lws_path`, which extends the Lua path where Lua searches for packages with another folder.


## Enabling the Examples Website

The exact steps to enable the website with the examples depend on the specifics of your NGINX
installation.

As a first step, you must have completed the LWS installation, as described in the main
[README](../README.md) document. In particular, the LWS module must be loaded in NGINX.

Second, you must copy the files of the website to a suitable location. In the configuration shown
above, that location is defined as `/var/www/lws-examples`. You can choose a different
location and adapt the configuration accordingly.

Third, you must enable the configuration. On some systems, this can be done by copying the
configuration to the `/etc/nginx/sites-available` folder and creating a corresponding symlink in
`/etc/nginx/sites-enabled`. The particularities depend on your system and NGINX installation.

Finally, you must instruct NGINX to reload the configuration, by issuing a command such as

```
sudo service nginx reload
```
If the website does not launch, it is advisable to check the error log of NGINX.


## Exploring the Examples Website

Once these steps are completed, you can point a browser to `http://localhost:8080/` and explore
the examples.

The first example is a classical "Hello, world!" example. Its source code looks like this:

```lua
-- Say hello
response.body:write("Hello, world, from ", _VERSION, "!")

-- Finish
response.status = lws.status.OK
response.headers["Content-Type"] = "text/plain; charset=UTF-8"
```

This Lua chunk instructs the web browser to display text similar to this:

```
Hello, world, from Lua 5.3!
```
