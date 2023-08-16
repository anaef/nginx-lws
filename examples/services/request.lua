-- Renders a variable
local function render_var (name, comment)
        local value = load("return " .. name, nil, "t", _ENV)()
        response.body:write(string.format("%-30s: %s", name, tostring(value)), "\n")
end

-- Render output
response.body:write("* HTTP Request", "\n\n")
render_var("request.method")
render_var("request.uri")
render_var("request.path")
render_var("request.path_info")
render_var("request.args")

-- Render the headers
response.body:write("\n\n", "* HTTP Request Headers", "\n\n")
for key, value in pairs(request.headers) do
	response.body:write(string.format("%-30s: %s", key, value), "\n")
end

-- Finish
response.status = lws.status.OK
response.headers["Content-Type"] = "text/plain"
response.headers["Cache-Control"] = "private, no-store";
