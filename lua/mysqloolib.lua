--[==[
	This library aims to provide an easier and less verbose way to use mysqloo
	Function overview:
	
	mysqloo.ConvertDatabase(database)
		Returns: the modified database
		Modifies an existing database to make use of the extended functionality of this library
		
	mysqloo.CreateDatabase(host, username, password [, database, port, socket])
		Returns: the newly created database instance
		Does the same as mysqloo.connect() but adds convenient functions provided by this library
		
	Query callbacks are of this structure:
	function callback([additionalArgs], query, status, dataOrError) end
		additionalArgs are any additional arguments that are passed after the callback in RunQuery and similar
		query is the query object that represents the started query
		status is true if the query executed successfully, false otherwise
		dataOrError is either the results returned by query:getData() if the query executed successfully
					or and error message if an error occured (use status to know which one)
					Note: dataOrError is nil for transaction if the transaction finished successfully
	
	Database:RunQuery(queryStr, [callback [, additionalArgs]])
		Parameters:
			queryStr: the query to run
			callback: the callback function that is called when the query is done
			additionalArgs: any args that will be passed to the callback function on success (see callback structure)
		Returns: the query that has been created and started
		Description: Creates and runs a mysqloo query using the specified queryStr and runs the provided
					 callback function when the query finished. If no callback function is provided then an error message is printed if the query errors
		Example: database:RunQuery("SELECT 1", function(query, status, data)
					PrintTable(data)
				 end)
		
	Database:PrepareQuery(queryStr, parameterValues, [callback [, additionalArgs])
		Parameters:
			queryStr: the query string to run with ? representing parameters to be passed in parameterValues
			parameterValues: a table containing values that are supposed to replace the ? in the prepared query
			additionalArgs: see Database:RunQuery()
		Returns: the prepared query that has been created and started
		Description: Creates and runs a mysqloo prepared query using the specified queryStr and parameters and runs the provided
					 callback function when the query finished. If no callback function is provided then an error message is printed if the query errors
		Example: database:PrepareQuery("SELECT ?, ?", {1, "a"}, function(query, status, data)
					PrintTable(data)
				 end)
				 
	Database:CreateTransaction()
		Parameters: none
		Returns: a transaction object
		
	Transaction:Query(queryStr)
		Parameters:
			queryStr: the query to run
		Returns: the query that has been added to the transaction
		Description: Same as Database:RunQuery() but doesn't take a callback and instead of starting the query
					 adds the query to the transaction
	
	Transaction:Prepare(queryStr, parameterValues)
		Parameters:
			queryStr: the query string to run with ? representing parameters to be passed in parameterValues
			parameterValues: a table containing values that are supposed to replace the ? in the prepared query
		Returns: the prepared query that has been added to the transaction
		Description: Same as Database:PrepareQuery() but doesn't take a callback and instead of starting the query
					 adds the query to the transaction
					 
	Transaction:Start(callback [, additionalArgs])
		Parameters:
			callback: The callback that is called when the transaction is finished
			additionalArgs: see Database:RunQuery()
		Returns: nothing
		Description: starts the transaction and calls the callback when done
					 If the transaction finishes successfully all queries that belong to it have been
					 executed successfully. If the transaction fails none of the queries will have had effect
					 Check https://en.wikipedia.org/wiki/ACID for more information
		
	Transaction example:
		local transaction = Database:CreateTransaction()
		transaction:Query("SELECT 1")
		transaction:Prepare("INSERT INTO `some_tbl` (`some_field`) VALUES(?)", {1})
		transaction:Query("SELECT * FROM `some_tbl` WHERE `id` = LAST_INSERT_ID()")
		transaction:Start(function(transaction, status, err)
			if (!status) then error(err) end
			PrintTable(transaction:getQueries()[1]:getData())
			PrintTable(transaction:getQueries()[3]:getData())
		end)
]==]

require("mysqloo")
if (mysqloo.VERSION != "9" || !mysqloo.MINOR_VERSION || tonumber(mysqloo.MINOR_VERSION) < 1) then
	MsgC(Color(255, 0, 0), "You are using an outdated mysqloo version\n")
	MsgC(Color(255, 0, 0), "Download the latest mysqloo9 from here\n")
	MsgC(Color(86, 156, 214), "https://github.com/syl0r/MySQLOO/releases")
	return
end

local db = {}
local dbMetatable = {__index = db}

//This converts an already existing database instance to be able to make use
//of the easier functionality provided by mysqloo.CreateDatabase
function mysqloo.ConvertDatabase(database)
	return setmetatable(database, dbMetatable)
end

//The same as mysqloo.connect() but adds easier functionality
function mysqloo.CreateDatabase(...)
	local db = mysqloo.connect(...)
	db:connect()
	return mysqloo.ConvertDatabase(db)
end

local function addQueryFunctions(query, func, ...)
	local oldtrace = debug.traceback()
	local args = {...}
	table.insert(args, query)
	function query.onAborted(qu)
		table.insert(args, false)
		table.insert(args, "aborted")
		if (func) then
			func(unpack(args))
		end
	end

	function query.onError(qu, err)
		table.insert(args, false)
		table.insert(args, err)
		if (func) then
			func(unpack(args))
		else
			ErrorNoHalt(err .. "\n" .. oldtrace .. "\n")
		end
	end

	function query.onSuccess(qu, data)
		table.insert(args, true)
		table.insert(args, data)
		if (func) then
			func(unpack(args))
		end
	end
end

function db:RunQuery(str, callback, ...)
	local query = self:query(str)
	addQueryFunctions(query, callback, ...)
	query:start()
	return query
end

local function setPreparedQueryArguments(query, values)
	if (type(values) != "table") then
		values = { values }
	end
	local typeFunctions = {
		["string"] = function(query, index, value) query:setString(index, value) end,
		["number"] = function(query, index, value) query:setNumber(index, value) end,
		["boolean"] = function(query, index, value) query:setBoolean(index, value) end,
	}
	//This has to be pairs instead of ipairs
	//because nil is allowed as value
	for k, v in pairs(values) do
		local varType = type(v)
		if (typeFunctions[varType]) then
			typeFunctions[varType](query, k, v)
		else
			query:setString(k, tostring(v))
		end
	end
end

function db:PrepareQuery(str, values, callback, ...)
	self.CachedStatements = self.CachedStatements or {}
	local preparedQuery = self.CachedStatements[str] or self:prepare(str)
	addQueryFunctions(preparedQuery, callback, ...)
	setPreparedQueryArguments(preparedQuery, values)
	preparedQuery:start()
	return preparedQuery
end

local transaction = {}
local transactionMT = {__index = transaction}

function transaction:Prepare(str, values)
	//TODO: Cache queries
	local preparedQuery = self._db:prepare(str)
	setPreparedQueryArguments(preparedQuery, values)
	self:addQuery(preparedQuery)
	return preparedQuery
end
function transaction:Query(str)
	local query = self._db:query(str)
	self:addQuery(query)
	return query
end

function transaction:Start(callback, ...)
	local args = {...}
	table.insert(args, self)
	function self:onSuccess()
		table.insert(args, true)
		if (callback) then
			callback(unpack(args))
		end
	end
	function self:onError(err)
		err = err or "aborted"
		table.insert(args, false)
		table.insert(args, err)
		if (callback) then
			callback(unpack(args))
		else
			ErrorNoHalt(err)
		end
	end
	self.onAborted = self.onError
	self:start()
end

function db:CreateTransaction()
	local transaction = self:createTransaction()
	transaction._db = self
	setmetatable(transaction, transactionMT)
	return transaction
end