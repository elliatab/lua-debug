local fs = require 'bee.filesystem'

local function getExtensionDirName(packageDir)
    local publisher,name,version
    for line in io.lines(packageDir .. '/package.json') do
        if not publisher then
            publisher = line:match('"publisher": "([^"]+)"')
        end
        if not name then
            name = line:match('"name": "([^"]+)"')
        end
        if not version then
            version = line:match('"version": "(%d+%.%d+%.%d+)"')
        end
    end
    if not publisher then
        error 'Cannot found `publisher` in package.json.'
    end
    if not name then
        error 'Cannot found `name` in package.json.'
    end
    if not version then
        error 'Cannot found `version` in package.json.'
    end
    return ('%s.%s-%s'):format(publisher,name,version)
end

local function copy_directory(from, to, filter)
    fs.create_directories(to)
    for fromfile in from:list_directory() do
        if fs.is_directory(fromfile) then
            copy_directory(fromfile, to / fromfile:filename(), filter)
        else
            if (not filter) or filter(fromfile) then
                print('copy', fromfile, to / fromfile:filename())
                fs.copy_file(fromfile, to / fromfile:filename(), true)
            end
        end
    end
end

local packageDir,sourceDir,extensionPath = ...
local extensionDirName = getExtensionDirName(packageDir)
local extensionDir = fs.path(extensionPath) / extensionDirName
if not fs.exists(extensionDir) then
    error(extensionDir .. "is not installed.")
end

copy_directory(fs.path(sourceDir), extensionDir)

print 'ok'
