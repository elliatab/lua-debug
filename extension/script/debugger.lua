local platform, path, luaapi = ...

local rt = "/runtime"
if platform == "windows" then
    if string.packsize "T" == 8 then
        rt = rt .. "/win64"
    else
        rt = rt .. "/win32"
    end
else
    assert(string.packsize "T" == 8)
    rt = rt .. "/" .. platform
end
if _VERSION == "Lua 5.4" then
    rt = rt .. "/lua54"
elseif _VERSION == "Lua 5.3" then
    rt = rt .. "/lua53"
else
    error(_VERSION .. " is not supported.")
end

local ext = platform == "windows" and "dll" or "so"
local remotedebug = path..rt..'/remotedebug.'..ext
if luaapi then
    assert(package.loadlib(remotedebug,'init'))(luaapi)
end
local rdebug = assert(package.loadlib(remotedebug,'luaopen_remotedebug'))()

local function dofile(filename, ...)
    local f = assert(io.open(filename))
    local str = f:read "a"
    f:close()
    local func = assert(load(str, "=(BOOTSTRAP)"))
    return func(...)
end

local dbg = dofile(path..'/script/start_debug.lua', rdebug,path,'/script/?.lua',rt..'/?.'..ext)
debug.getregistry()["lua-debug"] = dbg

return dbg
