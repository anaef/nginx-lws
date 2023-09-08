# Getting Started with LWS

LWS comes with a website with examples. You can find the site in the `/examples` folder of this
repository.


## Examples Website Configuration

The configuration provided with the examples is called `lws-examples.conf` and looks similar to
this:

```nginx
server {
	listen localhost:8080;
	
	location / {
		alias /var/www/lws-examples/static/;
	}

	location ~ /services/(\w+)(/.*)? {
		lws /var/www/lws-examples/services/$1.lua $2;
		lws_init /var/www/lws-examples/handlers/init.lua;
		lws_pre /var/www/lws-examples/handlers/pre.lua;
		lws_path "+/var/www/lws-examples/modules/?.lua;/var/www/lws-examples/modules/?/init.lua";
		lws_max_requests 1;
	}

	location /internal/ {
		alias /var/www/lws-examples/internal/;
		internal;
	}

	location /lws-monitor/ {
		lws_monitor;
	}
}
```

This configuration defines a web server with four locations:
- A root location that serves static content from the `static` subfolder.
- A `/services/` location that provides the web services implemented in Lua.
- A `/internal/` location only accessible via internal redirects.
- A `/lws-monitor/` location that provides read-write access to central LWS characteristics.

The `/services` location uses the central `lws` directive to enable the LWS handler. The directive
maps paths to files with main Lua chunks that implement a web service. Specifically, the path
`/services/{name}` is mapped to the main Lua chunk `/var/www/lws-examples/services/{name}.lua`
using variable `$1`, which corresponds to the first capture group of the location path, i.e.,
`(\w+)`. Optional extra path information following the service name is provided as path info to
the Lua code using the variable `$2`, which corresponds to the 2nd capture group of the location
path, i.e., `(/.*)?`.

Further used directives include:

- `lws_init`, which refers to a Lua chunk that initializes the state. This chunk is run once per
state.
- `lws_pre`, which refers to a Lua chunk that prepares a state for a request. This chunk is run
per request, before the main Lua chunk.
- `lws_path`, which sets the Lua path where Lua searches for packages. Due to the `+` sign, the
set path is appended to the default Lua path.

For more information, please refer to the [directives](Directives.md) documentation.


## Enabling the Examples Website

The exact steps to enable the website with the examples depend on the specifics of your NGINX
installation.

As a first step, you must complete the LWS installation as described in the
[installation](Installation.md) document. In particular, the LWS module must be loaded in NGINX.

Second, you must copy the website files to a suitable location. The configuration above defines
that location as `/var/www/lws-examples`. You can choose a different location and adapt the
configuration accordingly.

Third, you must enable the configuration. On some systems, this can be done by copying the
configuration to the `/etc/nginx/sites-available` folder and creating a corresponding symlink in
`/etc/nginx/sites-enabled`. The particularities depend on your system and NGINX installation.

Finally, you must instruct NGINX to reload its configuration by issuing a command such as

```
sudo service nginx reload
```

If the website does not launch, checking the NGINX error log is advisable.


## Exploring the Examples Website

Once these steps are completed, you can point a browser to `http://localhost:8080/` and explore
the examples.

The first example is a classical "Hello, world!". Its source code looks like this:

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
