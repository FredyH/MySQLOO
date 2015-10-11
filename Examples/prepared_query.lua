include("database.lua") -- check database.lua for an example of how to create database instances

local preparedQuery = db:prepare("INSERT INTO users (`steamid`, `nick`, `rank`, `playtime`, `sex`) VALUES(?, ?, ?, ?, ?)")
function preparedQuery:onSuccess(data)
	print("Rows inserted successfully!")
end

function preparedQuery:onError(err)
	print("An error occured while executing the query: " .. err)
end

preparedQuery:setString(1, "STEAM_0:0:123456")
preparedQuery:setString(2, "''``--lel") -- you don't need to escape the string if you use setString
preparedQuery:setNull(3)
preparedQuery:setNumber(4, 100)
preparedQuery:setBoolean(5, true)
preparedQuery:start()