-- Renders arguments
local function render_args (name, args)
	response.body:write("* ", name, "\n\n")
	if string.len(args) == 0 then
		response.body:write("No parameters found.", "\n")
	else
		local parsed_args = lws.parseargs(args)
		for key, value in pairs(parsed_args) do
			response.body:write(string.format("%-30s: %s", key, value), "\n")
		end
	end
	response.status = lws.status.OK
	response.headers["Content-Type"] = "text/plain"
	response.headers["Cache-Control"] = "private, no-store"
end

-- Renders content on GET
local function content ()
	render_args("HTTP Request Query Parameters", request.args)
end

-- Handles POST
local function action ()
	render_args("HTTP Request Body Parameters", request.body:read("*a"))
end

-- Handle methods
if request.method == "GET" then
	return content()
elseif request.method == "POST" then
	return action()
else
	return lws.status.METHOD_NOT_ALLOWED
end
