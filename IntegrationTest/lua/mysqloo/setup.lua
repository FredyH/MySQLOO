require("mysqloo")

function TestFramework:ConnectToDatabase()
	local db = mysqloo.connect(DatabaseSettings.Host, DatabaseSettings.Username, DatabaseSettings.Password, DatabaseSettings.Database, DatabaseSettings.Port)
	db:connect()
	db:wait()
	return db
end

function TestFramework:RunQuery(db, queryStr)
	local query = db:query(queryStr)
	query:start()
	function query:onError(err)
		error(err)
	end
	query:wait()
	return query:getData()
end