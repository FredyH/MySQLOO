//Put this into your server's lua/includes/modules/ folder to replace tmysql4 functionality with mysqloo
//This is only a temporary solution and the best would be to change to mysqloo completely
//A few incompatibilities:
//Mysql Bit fields are returned as numbers instead of a single char
//Mysql Bigint fields are returned as numbers
//This might pose a problem if you have a bigint field for steamid64
//Always make sure to cast that field to a string in the SELECT clause of your query
//Example: SELECT CAST(steamid64 as 'CHAR') as steamid64 FROM ...

require("mysqloo")
if (mysqloo.VERSION != "9") then
	error("using outdated mysqloo version")
end
tmysql = tmysql or {}
tmysql.Connections = tmysql.Connections or {}
local database = {}
local databaseMT = {__index = database}

function database:Escape(...)
	if (self.Disconnected) then error("database already disconnected") end
	return self:escape(...)
end

function database:Connect()
	self:connect()
	self:wait() //this is dumb
	//Unfortunately mysqloo only passes the error message to a callback
	//because waiting for the db to connect is really dumb
	//so there is no way to retrieve the actual error message here
	if (self:status() != mysqloo.DATABASE_CONNECTED) then
		return false, "[TMYSQL Wrapper]: Failed to connect to database"
	end
	table.insert(tmysql.Connections, self)
	return true
end

function database:Query(str, callback, ...)
	if (self.Disconnected) then error("database already disconnected") end
	local additionalArgs = {...}
	local qu = self:query(str)
	if (!callback) then
		qu:start()
		return
	end
	qu.onSuccess = function(qu, result)
		local results = {
			{
				status = true,
				error = nil,
				affected = qu:affectedRows(),
				lastid = qu:lastInsert(),
				data = result
			}
		}
		while(qu:hasMoreResults()) do
			result = qu:getNextResults()
			table.insert(results, {
				status = true,
				error = nil,
				affected = qu:affectedRows(),
				lastid = qu:lastInsert(),
				data = result
			})
		end
		table.insert(additionalArgs, results)
		callback(unpack(additionalArgs))
	end
	qu.onAborted = function(qu)
		local data = {
			status = false,
			error = "Query aborted"
		}
		table.insert(additionalArgs, {data})
		callback(unpack(additionalArgs))
	end
	qu.onError = function(qu, err)
		local data = {
			status = false,
			error = err
		}
		table.insert(additionalArgs, {data})
		callback(unpack(additionalArgs))
	end
	qu:start()
end
	
function database:SetCharacterSet(name)
	return self:setCharacterSet(name)
end

function database:Disconnect()
	if (self.Disconnected) then error("database already disconnected") end
	self:abortAllQueries()
	table.RemoveByValue(tmysql.Connections, self)
	self.Disconnected = true
	self:disconnect()
end

function database:Option(option, value)
	if (self.Disconnected) then error("database already disconnected") end
	if (option == bit.lshift(1, 16)) then	
		self:setMultiStatements(tobool(value))
	else
		print("[TMYSQL Wrapper]: Unsupported tmysql option")
	end
end

function tmysql.GetTable()
	return tmysql.Connections
end

//Clientflags are ignored, multistatements are always enabled by default
function tmysql.initialize(host, user, password, database, port, unixSocketPath, clientFlags)
	local db = mysqloo.connect(host, user, password, database, port, unixSocketPath)
	setmetatable(db, databaseMT)
	local status, err = db:Connect()
	if (!status) then
		return nil, err
	end
	return db, err
end

function tmysql.Create(host, user, password, database, port, unixSocketPath, clientFlags)
	local db = mysqloo.connect(host, user, password, database, port, unixSocketPath)
	setmetatable(db, databaseMT)
	return db
end

tmysql.Connect = tmysql.initialize
