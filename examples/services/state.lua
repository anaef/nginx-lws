-- Renders a variable w/ comment
local function render_var (name, comment)
	local value = load("return " .. name, nil, "t", _ENV)()
	response.body:write(string.format("%-30s: %-10s  -- %s", name, tostring(value), comment),
			"\n")
end

-- Executes and renders a statement
local function execute_statement (statement)
	response.body:write("\n\n- After statement '", statement, "':\n")
	load(statement, nil, "t", _ENV)()
end

-- Render variables from global and request environment
response.body:write("* Lua State Environments", "\n\n")
render_var("_G.INITIALIZED", "set once in the global environment by init.lua")
render_var("INITIALIZED",  "undefined in the request environment; gets global value")
render_var("_G.NOW", "undefined in global environment, thus nil")
render_var("NOW", "set in the request environment by pre.lua")

-- Set a variable in the request environment
execute_statement("INITIALIZED = false")
render_var("_G.INITIALIZED", "unchanged")
render_var("INITIALIZED", "now overwritten in the request environment")

-- Set a variable in the global environment (not generally recommended)
execute_statement("_G.INITIALIZED = false")
render_var("_G.INITIALIZED", "now changed in the global environment; not generally recommended")
render_var("INITIALIZED", "unchanged")

-- Revert the change
execute_statement("_G.INITIALIZED = true")
render_var("_G.INITIALIZED", "reverted")
render_var("INITIALIZED", "unchanged")

-- Finish
response.status = lws.status.OK
response.headers["Content-Type"] = "text/plain; charset=UTF-8"
