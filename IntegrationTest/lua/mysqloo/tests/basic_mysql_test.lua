TestFramework:RegisterTest("[Basic] selecting 1 should return 1", function(test)
	local db = TestFramework:ConnectToDatabase()
	local query = db:query("SELECT 3 as test")
	function query:onSuccess(data)
		test:shouldHaveLength(data, 1)
		test:shouldBeEqual(data[1]["test"], 3)
		test:Complete()
	end
	function query:onError(err)
		test:Fail(err)
	end
	query:start()
	query:wait()
end)