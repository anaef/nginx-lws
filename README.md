# Lua Web Services for NGINX

lws-nginx is a module for the [NGINX](https://nginx.org/) server that supports web services
written in Lua, running directly in the server.

Some central design considerations for lws-nginx are the following:

- **Use [PUC-Lua](https://www.lua.org/).** PUC-Lua is the original implementation of Lua
maintained by the language's creators.

- **Allow Lua web services to block.** The lws-nginx module uses a thread pool that runs Lua
services asynchronously without blocking the NGINX event processing loop. As large parts of the
existing Lua ecosystem are *not* non-blocking, this design allows them to be used as is, on the
condition that their libraries are conditionally thread-safe.

- **Make efficient use of Lua states.** Loading Lua code can take a significant amount of time.
For this reason, lws-nginx can reuse Lua states for subsequent requests. A broad range of
[directives](doc/Directives.md) allows for control over the lifecycle of Lua states.

- **Ensure manageability.** The lws-nginx module includes a [monitor](doc/Monitor.md) web API that
provides access to central LWS characteristics, including a built-in function profiler. A
self-contained web page that displays periodically updated data from the LWS monitor and allows
for controlling the profiler is also provided.

- **Focus on web services.** The purpose of lws-nginx is to implement web services in Lua. This
focus streamlines the design of lws-nginx. For other extension areas in NGINX (including
rewriting, access, and filters), numerous highly configurable modules exist that address these
functions.


## Discussion

This section briefly discusses the motivations for using PUC-Lua instead of LuaJIT and allowing
Lua web services to block instead of using an event-based, non-blocking architecture.

While [LuaJIT](https://luajit.org/) is undoubtedly an amazing feat of engineering with impressive
performance, it remains based on Lua 5.1 with select extensions. The latest PUC-Lua release is
5.4. PUC-Lua has added new language features over the years, including 64-bit integers, bit
operators, and variable attributes, which are not directly supported in LuaJIT. Perhaps more
worryingly, writing "fast" LuaJIT code promotes using language idioms that are amenable to its
optimization while eschewing language features that are "slow".[^1] It is not ideal if a JIT
compiler informs how a language is used.

In practice, the PUC-Lua VM is more than fast enough for a broad range of workloads. If its
performance is deemed insufficient for a particular function of a web service, implementing that
function in C is always possible. Furthermore, the research team at PUC-Rio is working on
ahead-of-time compilation through Pallene.[^2]

Regarding server architectures, there is substantial research and discussion on
non-blocking event architectures vs. multi-threaded architectures.[^3] [^4] [^5] [^6] [^7] [^8]

Ultimately, this is a trade-off between resource use and complexity. An event processing loop is
arguably less demanding on resources than threads. However, threads' resource demand has been
decreasing over the years with advances in operating systems. On the other hand, the complexity
of implementing web services and libraries is arguably lower if they can block instead of
implementing the logic for cooperative multitasking through yielding and asynchronous
continuation. Less complexity generally means fewer bugs. The lws-nginx module further avoids
complex synchronization logic by keeping the dispatch logic to the thread pool in the
single-threaded event processing loop of NGINX.

Given these considerations and that large parts of the existing Lua ecosystem are *not*
non-blocking, using threads seems a reasonable, pragmatic approach today.


## Documentation

Please browse the extensive documentation in the [doc](doc) folder.


## Status

lws-nginx is a work in progress.


## Limitations

lws-nginx has been tested with the following software versions:

* Ubuntu 20.04.6 LTS, Lua 5.3.3, and NGINX 1.18.0
* Unbutu 22.04.3 LTS, Lua 5.4.4, and NGINX 1.18.0

Your mileage may vary with other software versions.

Lua libraries used with lws-nginx must be conditionally thread-safe.


## Trademarks

NGINX is a registered trademark of F5, Inc.

All other trademarks are the property of their respective owners.


## License

lws-nginx is released under the MIT license. See LICENSE for license terms.


[^1]: Hugo Musso Gualandi. The Pallene Programming Language. 2020.
[Link](http://www.lua.inf.puc-rio.br/publications/2020-HugoGualandi-phd-thesis.pdf).

[^2]: Roberto Ierusalimschy. What about Pallene? Lua Workshop 2022. 2022.
[Link](https://www.lua.org/wshop22/Ierusalimschy.pdf).

[^3]: Paolo Maresca. Scalable I/O: Events- Vs Multithreading-based. 2016.
[Link](https://thetechsolo.wordpress.com/2016/02/29/scalable-io-events-vs-multithreading-based/).

[^4]: Paul Tyma. Thousands of Threads and Blocking I/O. 2008.
[Link](https://silo.tips/download/thousands-of-threads-and-blocking-i-o).

[^5]: Rob von Behren et al. Why Events Are A Bad Idea (for high-concurrency servers). In
Proceedings of HotOS IX: The 9th Workshop on Hot Topics in Operating Systems. 2003.
[Link](https://www.usenix.org/legacy/events/hotos03/tech/full_papers/vonbehren/vonbehren.pdf).

[^6]: Atul Adya et al. Cooperative Task Management Without Manual Stack Management. In
Proceedings of the 2002 USENIX Annual Technical Conference. 2002.
[Link](https://www.usenix.org/legacy/publications/library/proceedings/usenix02/full_papers/adyahowell/adyahowell.pdf).

[^7]: Matt Welsh et al. SEDA: An Architecture for Well-Conditioned Scalable Internet Services.
In Symposium on Operating Systems. 2001.
[Link](http://www.sosp.org/2001/papers/welsh.pdf).

[^8]: John Ousterhout. Why Threads Are A Bad Idea (for most purposes). 1996.
[Link](https://web.stanford.edu/~ouster/cgi-bin/papers/threads.pdf).
