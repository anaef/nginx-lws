_ENV = setmetatable({ }, { __index = _G })

-- Returns a a message with the current time
function get_time ()
	return string.format("It is %s.", os.date())
end

-- Return module
return _ENV
