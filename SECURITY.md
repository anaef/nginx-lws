# Security

## Security Posture

LWS for NGINX assumes that NGINX and its API, operator configuration, Lua code, the Lua runtime, and
loaded libraries are trusted. HTTP requests are considered potentially attacker-controlled. It is
not designed to sandbox untrusted Lua code or isolate mutually untrusted parties. Applications and
operators are responsible for application access control, transport protection, restricting monitor
access, process isolation, resource limits, dependency compatibility, Lua code, and Lua-library
thread safety. Reports that require a violation of these assumptions, or concern defects in NGINX or
other dependencies, are outside the project's security scope, but may still be considered as
ordinary robustness or correctness issues.
