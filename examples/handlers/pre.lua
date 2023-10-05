-- Initialize the request environment
NOW = os.time()  -- This is set in the request environment

-- Evaluates an Lua chunk in the request environment
function eval (expr)
	if _VERSION > "Lua 5.1" then
		return load(expr, nil, "t", _ENV)()
	else
		local f = loadstring(expr)
		debug.setfenv(f, debug.getfenv(eval))
		return f()
	end
end
