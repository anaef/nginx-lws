-- Returns the current time
local function time ()
	-- Prepare body (without a proper JSON serializer, to keep the example self-contained)
	local body = string.format("{ \"time\": %d }", NOW)

	-- Send response
	response.status = lws.status.OK
	response.headers["Content-Type"] = "application/json"
	response.body:write(body)
end

-- Handle methods
if request.method == "GET" then
	return time()
else
	return lws.status.METHOD_NOT_ALLOWED
end
