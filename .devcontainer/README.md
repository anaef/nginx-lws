# NGINX LWS DevContainer

The DevContainer uses Ubuntu 24.04, NGINX 1.24.0, Lua 5.4, and GCC. The repository is mounted at
`/workspaces/nginx-lws`; all NGINX sources, builds, installations, runtime state, and logs are
disposable container data.

The container itself is kept alive by Docker's init and the DevContainer keepalive process. NGINX
is deliberately not PID 1, so rebuilding the module and replacing the NGINX master does not close
the development shell.

The examples site is forwarded to [http://localhost:8080](http://localhost:8080).

## Layout

| Path | Purpose |
| --- | --- |
| `/workspaces/nginx-lws` | Host-mounted LWS source |
| `/opt/nginx-1.24.0` | Pinned upstream NGINX source |
| `/opt/nginx-build/normal` | Normal dynamic-module build output |
| `/opt/nginx-lws` | Normal development module installation |
| `/var/log/nginx-lws` | Normal development and Valgrind logs |
| `/run/nginx-lws` | Normal development PID and temporary files |
| `/opt/nginx-build/asan` | AddressSanitizer build output |
| `/opt/nginx` | Complete private AddressSanitizer NGINX installation and logs |

No named or persistent volumes are used. Reopening or restarting the existing container preserves
its build trees; rebuilding the DevContainer recreates them from the pinned source.

## Normal development

The normal runtime uses Ubuntu's `/usr/sbin/nginx` with the locally built module. It starts
automatically whenever the DevContainer starts.

```bash
lws-build
```

`lws-build` configures and compiles the dynamic module, installs it atomically, validates the NGINX
configuration, and gracefully replaces the NGINX master. It restores the previous module if
validation or startup fails.

Lua services use `lws_max_requests 1`, so editing Lua or static files requires only a browser
refresh. For configuration-only changes, use:

```bash
lws-nginx reload
```

Other lifecycle and log commands are:

```bash
lws-nginx start
lws-nginx stop
lws-nginx restart
lws-nginx test
lws-nginx status
lws-nginx logs
lws-nginx logs access
lws-nginx logs all
```

## AddressSanitizer stress test

```bash
lws-asan
```

This builds and installs an independent, ASan-instrumented NGINX under `/opt/nginx`. It stops the
normal server, sends 20,000 requests with parallelism 12, waits for the configured ten-second
Lua-state idle timeout, gracefully stops the ASan server, checks the sanitizer output, and restores
the normal server.

The stress configuration intentionally reuses and constrains Lua states:

- `lws_max_requests 512`
- `lws_max_states 4`
- `lws_timeout 10s`
- `lws_max_memory 272K`

The request count and parallelism can be changed:

```bash
lws-asan 50000 16
```

ASan, access, and NGINX error logs are retained under `/opt/nginx/logs`.

LeakSanitizer remains enabled for the server lifecycle. The test suppresses only NGINX 1.24.0's
known PCRE2 compile-context allocation from regex-location setup; LWS allocations are not
suppressed.

## Valgrind

```bash
lws-valgrind
```

This updates the normal module, stops the normal daemon, and runs NGINX interactively under
Valgrind with `master_process off`. Exercise the examples from the browser or another terminal and
press Ctrl-C in the Valgrind terminal when finished. The helper translates Ctrl-C into a graceful
NGINX shutdown so Valgrind can complete its report, then restores the normal server.

The completed Valgrind report is displayed in the terminal and written to
`/var/log/nginx-lws/valgrind.log`.
The helper suppresses NGINX 1.24.0's known single-process event-loop, accepted-connection,
environment, and PCRE2 teardown allocations; it does not suppress LWS or Lua allocation stacks.
