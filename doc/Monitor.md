# LWS Monitor

The LWS monitor provides read-write access to central LWS characteristics. It is configured
through the `lws_monitor` [directive](Directives.md).

> [!WARNING]
> The LWS monitor should *not* be enabled at locations that are publicly accessible. Enabling
> the monitor at a location without appropriate access controls is a security risk.


## `GET` Method

The `GET` method returns a JSON document as follows:

```json
{
	"states_n": 0,
	"requests_n": 0,
	"memory_used": 0,
	"request_count": 0,
	"profiling": 0
}
```

The following table describes the keys of the document.

| Key | Type | Description |
| --- | --- | --- |
| `states_n` | `number` | Number of Lua states (active + inactive) |
| `requests_n` | `number` | Number of queued requests |
| `memory_used` | `number` | Memory used by Lua, in bytes |
| `request_count` | `number` | Total number of requests served |
| `profiling` | `number` | Profiler status; `0` = disabled; `1` = enabled |


## `POST` Method

The `POST` method modifies the status of the LWS monitor. The content type of the request body
must be `application/x-www-form-urlencoded`. The following table describes the keys that can be
modified.

| Key | Description |
| --- | --- |
| `profiling` | Profiler status; `0` = disabled; `1` = enabled |

The response has a 200 OK status if the modification succeeded, a 400 Bad Request status if an
invalid value was provided, or a 409 Conflict status if the modification failed. The latter can
happen due to a concurrent modification.


## Examples Website

The [examples website](GettingStarted.md) includes a self-contained web page that periodically
queries the monitor for display, and allows for enabling and disabling the profiler.
