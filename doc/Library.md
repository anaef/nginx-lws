# LWS Library

The LWS library provides functions and other values for implementing web services with LWS. The
library is loaded automatically.


## log(severity, message)

Logs a message in the error log of the web server. The argument *message* must be a string value,
and the argument *severity* can take the values `emerg`, `alert`, `crit`, `err`, `warn`, `notice`,
`info`, and `debug`; it defaults to `info`.


## redirect(location [, args])

Schedules an internal redirect to *location*. If *location* starts with `@`, it refers to
a named location. Otherwise, *location* refers to a path, and *args* are optional query arguments.
The request becomes internal and can use locations marked with the `internal` directive. A Lua
chunk should return after scheduling an internal redirect.


## parseargs(args)

Parses the query args in *args* and returns them as a table. For repeated keys, only the final value
is provided.
