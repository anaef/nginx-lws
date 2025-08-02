# LWS Release Notes


## Release 1.1.0

- Add HTTP response streaming.
- Add experimental 32-bit support.
- Change default Lua version to 5.4.
- Fix table cap initialization resulting in unlimitted stat cache.


## Release 1.0.0 (2024-12-30)

- Documentation update.


## Release 0.9.5 (2024-09-10)

- Fix memory accounting error with the lws_max_memory directive.


## Release 0.9.4 (2024-08-28)

- Fix non-cancelable state timer by making it cancelable to support graceful shutdown.
- Fix incorrect memory reference when resolving main configuration during shutdown.
- Fix potential double-free of states with the lws_max_time directive.
- The NGINX not-modified processing has been turned off.


## Release 0.9.3 (2024-07-16)

- Fix unassigned last modified time in response headers.


## Release 0.9.2 (2024-04-12)

- A Dockerized version of the website with the examples has been added. Thank you, Gonzalo Luque.
- Add unfolding of Set-Cookie response headers.
- Change size of monitor shared memory zone to 512 kB (up from 128 kB).
- Improve memory formatting on monitor example page.
- Documentation updates.


## Release 0.9.1 (2023-10-06)

- Add compatibility with Lua 5.1 and Lua 5.2.
- Fix compilation errors against more recent versions of NGINX.


## Release 0.9.0 (2023-09-20)

- Initial public release.
