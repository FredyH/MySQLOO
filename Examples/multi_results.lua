include("database.lua") -- check database.lua for an example of how to create database instances

local query = db:query("SELECT 1, 2, 3; SELECT 4, 5, 6; SELECT 7, 8, 9") -- In mysqloo 9 a query can be started before the database is connected
function query:onSuccess(data)
	local row = data[1]
	for k,v in pairs(row) do
		print(v) -- should print 1, 2, 3 in any order
	end
	while(self:hasMoreResults()) do -- should be true twice
		row = self:getNextResults()[1]
		for k,v in pairs(row) do
			print(v) -- should print 4, 5, 6, 7, 8, 9 in any order
		end
	end
end

function query:onError(err)
	print("An error occured while executing the query: " .. err)
end

query:start()