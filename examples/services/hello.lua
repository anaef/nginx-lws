-- Say hello
response.body:write("Hello, world, from ", _VERSION, "!")

-- Finish
response.status = lws.status.OK
response.headers["Content-Type"] = "text/plain; charset=UTF-8"
