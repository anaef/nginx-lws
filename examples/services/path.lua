-- Renders a variable
local function render_var (name, comment)
	local value = load("return " .. name, nil, "t", _ENV)()
	response.body:write(string.format("%-30s: %s", name, tostring(value)), "\n")
end

-- Render output
response.body:write("* Modules and Paths", "\n\n")
render_var("package.path")
render_var("package.cpath")
render_var("require(\"example\")")
render_var("require(\"example\").get_time")
render_var("require(\"example\").get_time()")

-- Finish
response.status = lws.status.OK
response.headers["Content-Type"] = "text/plain; charset=UTF-8"
response.headers["Cache-Control"] = "private, no-store"
