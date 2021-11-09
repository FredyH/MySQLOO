TestFramework:RegisterTest("[Database] should return info correctly", function(test)
	local db = TestFramework:ConnectToDatabase()
	local serverInfo = db:serverInfo()
	local hostInfo = db:hostInfo()
	local serverVersion = db:serverVersion()
	test:shouldBeGreaterThan(#serverInfo, 0)
	test:shouldBeGreaterThan(#hostInfo, 0)
	test:shouldBeGreaterThan(serverVersion, 0)
	test:Complete()
end)

TestFramework:RegisterTest("[Database] queue size should return correct size", function(test)
	local db = TestFramework:ConnectToDatabase()
	test:shouldBeEqual(db:queueSize(), 0)
	local query1 = db:query("SELECT SLEEP(0.5)")
	local query2 = db:query("SELECT SLEEP(0.5)")
	local query3 = db:query("SELECT SLEEP(0.5)")
	function query1:onSuccess()
		test:shouldBeEqual(db:queueSize(), 1) //1 because the next was already started at this point
	end
	function query2:onSuccess()
		test:shouldBeEqual(db:queueSize(), 0) //0 because the next was already started at this point
		test:Complete()
	end
	function query3:onSuccess()
		test:shouldBeEqual(db:queueSize(), 0)
		test:Complete()
	end
	query1:start()
	query2:start()
	query3:start()
	test:shouldBeGreaterThan(db:queueSize(), 1)
end)

TestFramework:RegisterTest("[Database] should abort all queries correctly", function(test)
	local db = TestFramework:ConnectToDatabase()
	local query1 = db:query("SELECT SLEEP(0.5)")
	local query2 = db:query("SELECT SLEEP(0.5)")
	local query3 = db:query("SELECT SLEEP(0.5)")
	local abortedCount = 0
	local f = function(q)
		abortedCount = abortedCount + 1
		if (abortedCount == 2) then
			test:Complete()
		end
	end
	query1.onAborted = f
	query2.onAborted = f
	query3.onAborted = f
	query1:start()
	query2:start()
	query3:start()
	local amountAborted = db:abortAllQueries()
	test:shouldBeGreaterThan(amountAborted, 1) //The one already processing might not be aborted
end)

TestFramework:RegisterTest("[Database] should escape a string correctly", function(test)
	local db = TestFramework:ConnectToDatabase()
	local escapedStr = db:escape("t'a")
	test:shouldBeEqual(escapedStr, "t\\'a")
	test:Complete()
end)

TestFramework:RegisterTest("[Database] should return correct status", function(test)
	local db = mysqloo.connect(DatabaseSettings.Host, DatabaseSettings.Username, DatabaseSettings.Password, DatabaseSettings.Database, DatabaseSettings.Port)
	test:shouldBeEqual(db:status(), mysqloo.DATABASE_NOT_CONNECTED)
	db:connect()
	function db:onConnected()
		test:shouldBeEqual(db:status(), mysqloo.DATABASE_CONNECTED)

		db:disconnect(true)
		test:shouldBeEqual(db:status(), mysqloo.DATABASE_NOT_CONNECTED)
		test:Complete()
	end
end)

TestFramework:RegisterTest("[Database] should call onConnected callback correctly", function(test)
	local db = mysqloo.connect(DatabaseSettings.Host, DatabaseSettings.Username, DatabaseSettings.Password, DatabaseSettings.Database, DatabaseSettings.Port)
	function db:onConnected()
		test:Complete()
	end
	db:connect()
end)

TestFramework:RegisterTest("[Database] should call onConnectionFailed callback correctly", function(test)
	local db = mysqloo.connect(DatabaseSettings.Host, DatabaseSettings.Username, "incorrect_password", DatabaseSettings.Database, DatabaseSettings.Port)
	function db:onConnectionFailed(err)
		test:shouldBeGreaterThan(#err, 0)
		test:Complete()
	end
	db:connect()
end)

TestFramework:RegisterTest("[Database] should ping correctly", function(test)
	local db = TestFramework:ConnectToDatabase()
	test:shouldBeEqual(db:ping(), true)
	test:Complete()
end)

TestFramework:RegisterTest("[Database] allow setting only valid character set", function(test)
	local db = TestFramework:ConnectToDatabase()
	test:shouldBeEqual(db:setCharacterSet("utf8"), true)
	test:shouldBeEqual(db:setCharacterSet("ascii"), true)
	test:shouldBeEqual(db:setCharacterSet("invalid_name"), false)
	test:Complete()
end)

TestFramework:RegisterTest("[Database] wait for queries when disconnecting", function(test)
	local db = TestFramework:ConnectToDatabase()
	local qu = db:query("SELECT SLEEP(1)")
	local wasCalled = false
	function qu:onSuccess()
		wasCalled = true
	end
	qu:start()
	db:disconnect(true)
	test:shouldBeEqual(wasCalled, true)
	test:Complete()
end)