//A simple connection pool for mysqloo9
//Use mysqloo.CreateConnectionPool(connectionCount, host, username, password [, database, port, socket]) to create a connection pool
//That automatically balances queries between several connections and acts like a regular database instance

require("mysqloo")
if (mysqloo.VERSION != "9") then
	error("Using outdated mysqloo version")
end

local pool = {}
local poolMT = {__index = pool}


function mysqloo.CreateConnectionPool(conCount, ...)
	if (conCount < 1) then
		error("Has to contain at least one connection")
	end
	local newPool = setmetatable({}, poolMT)
	newPool._Connections = {}
	local function failCallback(db, err)
		print("Failed to connect all db pool connections:")
		print(err)
	end
	for i = 1, conCount do
		local db = mysqloo.connect(...)
		db.onConnectionFailed = failCallback
		db:connect()
		table.insert(newPool._Connections, db)
	end
	return newPool
end

function pool:queueSize()
	local count = 0
	for k,v in pairs(self._Connections) do
		count = count + v:queueSize()
	end
	return count
end

function pool:abortAllQueries()
	for k,v in pairs(self._Connections) do
		v:abortAllQueries()
	end
end

function pool:getLeastOccupiedDB()
	local lowest = nil
	local lowestCount = 0
	for k, db in pairs(self._Connections) do
		local queueSize = db:queueSize()
		if (!lowest || queueSize < lowestCount) then
			lowest = db
			lowestCount = queueSize
		end
	end
	if (!lowest) then
		error("failed to find database in the pool")
	end
	return lowest
end

local overrideFunctions = {"escape", "query", "prepare", "createTransaction", "status", "serverVersion", "hostInfo", "ping"}

for _, name in pairs(overrideFunctions) do
	pool[name] = function(pool, ...)
		local db = pool:getLeastOccupiedDB()
		return db[name](db, ...)
	end
end