-- This code is run once per request, with a request environment
assert(_ENV ~= _G, "wrong environment")

-- Initialize the request environment
NOW = os.time()  -- This is thus set in the request environment
