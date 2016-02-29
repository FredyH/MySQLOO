include("database.lua") -- check database.lua for an example of how to create database instances

-- The transaction makes sure that either all of the queries are executed successfully, or none of them have any effect
-- If you want to read more about the concept of transactions, you can do it here: https://en.wikipedia.org/wiki/ACID
local transaction = db:createTransaction()

local query1 = db:query("UPDATE users SET cash = cash - 123 WHERE userid = 5")
local query2 = db:query("UPDATE users SET bank = bank + 123 WHERE userid = 5")

transaction:addQuery(query1) -- This also works with PreparedQueries
transaction:addQuery(query2)

-- Executed if and only if all of the queries in the transaction were executed successfully
function transaction:onSuccess()
	local allQueries = transaction:getQueries()
	local query1Duplicate = allQueries[1] -- You can either use this function or save a reference to the query
	print("Transaction completed successfully")
	print("Affected rows: " .. query1:affectedRows())
end

-- Executed if any of the queries in the transaction fail
function transaction:onError(err)
	print("Transaction failed: " .. err)
end

transaction:start()