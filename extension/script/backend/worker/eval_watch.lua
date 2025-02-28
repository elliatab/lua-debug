local source, level = ...
level = level + 2

local f = assert(debug.getinfo(level,"f").func, "can't find function")
local uv = {}
local uv_id = {}
local local_id = {}
local i = 1
while true do
	local name, value = debug.getlocal(level, i)
	if name == nil then
		break
	end
	if name:byte() ~= 40 then	-- '('
		uv[#uv+1] = name
		local_id[name] = value
	end
	i = i + 1
end
local i = 1
while true do
	local name = debug.getupvalue(f, i)
	if name == nil then
		break
	end
	uv_id[name] = i
	uv[#uv+1] = name
	i = i + 1
end
local full_source
if #uv > 0 then
	full_source = ([[
local $ARGS
return function(...)
return $SOURCE
end]]):gsub("%$(%w+)", {
	ARGS = table.concat(uv, ","),
	SOURCE = source,
})
else
	full_source = ([[
return function(...)
return $SOURCE
end]]):gsub("%$(%w+)", {
	SOURCE = source,
})
end
local func = assert(load(full_source, '=(eval)'))()
local i = 1
while true do
	local name = debug.getupvalue(func, i)
	if name == nil then
		break
	end
	local upvalue_id = uv_id[name]
	if upvalue_id then
		local upname, upvalue = debug.getupvalue(f, upvalue_id)
		if upname ~= nil then
			debug.setupvalue(func, i, upvalue)
		end
	end
	local local_value = local_id[name]
	if local_value then
		debug.setupvalue(func, i, local_value)
	end
	i = i + 1
end
local vararg, v = debug.getlocal(level, -1)
if vararg then
	local vargs = { v }
	local i = 2
	while true do
		vararg, v = debug.getlocal(level, -i)
		if vararg then
			vargs[i] = v
		else
			break
		end
		i=i+1
	end
	return func(table.unpack(vargs))
else
	return func()
end
