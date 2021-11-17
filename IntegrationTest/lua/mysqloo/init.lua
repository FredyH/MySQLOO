include("testframework.lua")
include("setup.lua")


local files = file.Find("mysqloo/tests/*.lua", "LUA")
for _,f in pairs(files) do
	include("tests/" .. f)
end