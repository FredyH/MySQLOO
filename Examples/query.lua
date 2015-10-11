include("database.lua") -- check database.lua for an example of how to create database instances

local query = db:query("SELECT 1, 2, 3") -- In mysqloo 9 a query can be started before the database is connected
function query:onSuccess(data)
	local row = data[1]
	for k,v in pairs(row) do
		print(v) -- should print 1, 2, 3
	end
end

function query:onError(err)
	print("An error occured while executing the query: " .. err)
end

query:start()