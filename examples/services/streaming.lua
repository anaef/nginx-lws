-- Stream response
response.status = lws.status.OK
response.headers["Content-Type"] = "text/plain"
response.headers["X-Content-Type-Options"] = "nosniff"
for i = 1, 10 do
	response.body:write(i, "\n")
	response.body:flush()
end
