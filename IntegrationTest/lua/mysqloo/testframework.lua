

TestFramework = TestFramework or {}
TestFramework.RegisteredTests = TestFramework.RegisteredTests or {}

local TestMT = {}
TestMT.__index = TestMT

function TestFramework:RegisterTest(name, f)
	local tbl = setmetatable({}, {__index = TestMT})
	tbl.TestFunction = f
	tbl.Name = name
	for k,v in pairs(TestFramework.RegisteredTests) do
		if (v.Name == name) then
			-- Replacing the old test instead of adding it
			TestFramework.RegisteredTests[k] = tbl
			print("Replacing previously registered test ", name)
			return
		end
	end
	table.insert(TestFramework.RegisteredTests, tbl)
	print("Registered test ", name)
end

function TestFramework:RunNextTest()
	TestFramework.CurrentIndex = (TestFramework.CurrentIndex or 0) + 1
	TestFramework.TestTimeout = CurTime() + 3
	local test = TestFramework.RegisteredTests[TestFramework.CurrentIndex]
	TestFramework.CurrentTest = test
	if (!test) then
		TestFramework:OnCompleted()
	else
		test:Run()
	end
end

function TestFramework:CheckTimeout()
	if (!TestFramework.CurrentTest) then return end
	if (CurTime() > TestFramework.TestTimeout) then
		TestFramework.CurrentTest:Fail("TIMEOUT")
	end
end

hook.Add("Think", "TestFrameworkTimeoutCheck", function()
	TestFramework:CheckTimeout()
end)

function TestFramework:ReportResult(success)
	TestFramework.TestCount = (TestFramework.TestCount or 0) + 1
	if (success) then
		TestFramework.SuccessCount = (TestFramework.SuccessCount or 0) + 1
	else
		TestFramework.FailureCount = (TestFramework.FailureCount or 0) + 1
	end
end

function TestFramework:OnCompleted()
	print("[MySQLOO] Tests completed")
	MsgC(Color(255, 255, 255), "Completed: ", Color(30, 230, 30), TestFramework.SuccessCount, Color(255, 255, 255), " Failures: ", Color(230, 30, 30), TestFramework.FailureCount, "\n")

	for j = 0, 3 do
		timer.Simple(j * 0.5, function()
			for i = 1, 100 do
				collectgarbage("collect")
			end
		end)
	end
	timer.Simple(2, function()
		for i = 1, 100 do
			collectgarbage("collect")
		end
		local diffBefore = TestFramework.AllocationCount - TestFramework.DeallocationCount
		local diffAfter = mysqloo.allocationCount() - mysqloo.deallocationCount()
		local success = TestFramework.FailureCount == 0
		if (diffAfter > diffBefore) then
			MsgC(Color(255, 255, 255), "Found potential memory leak with ", diffAfter - diffBefore, " new allocations that were not freed\n")
			success = false
		else
			MsgC(Color(255, 255, 255), "All allocated objects were freed\n")
		end


		diffBefore = TestFramework.ReferenceCreatedCount - TestFramework.ReferenceFreedCount
		diffAfter = mysqloo.referenceCreatedCount() - mysqloo.referenceFreedCount()
		if (diffAfter > diffBefore) then
			MsgC(Color(255, 255, 255), "Found potential memory leak with ", diffAfter - diffBefore, " new references created that were not freed\n")
			success = false
		else
			MsgC(Color(255, 255, 255), "All created references were freed\n")
		end


		MsgC(Color(255, 255, 255), "Lua Heap Before: ", TestFramework.LuaMemory, " After: ", collectgarbage("count"), "\n")
		hook.Run("IntegrationTestsCompleted", success)
	end)
end

function TestFramework:Start()
	for i = 1, 5 do
		collectgarbage("collect")
	end
	TestFramework.CurrentIndex = 0
	TestFramework.SuccessCount = 0
	TestFramework.FailureCount = 0
	TestFramework.ReferenceCreatedCount = mysqloo.referenceCreatedCount()
	TestFramework.ReferenceFreedCount = mysqloo.referenceFreedCount()
	TestFramework.AllocationCount = mysqloo.allocationCount()
	TestFramework.DeallocationCount = mysqloo.deallocationCount()
	TestFramework.LuaMemory = collectgarbage("count")
	TestFramework:RunNextTest()
end

function TestMT:Fail(reason)
	if (self.Completed) then return end
	self.Completed = true
	MsgC(Color(230, 30, 30), "FAILED\n")
	MsgC(Color(230, 30, 30), "Error: ", reason, "\n")
	TestFramework:ReportResult(false)
	TestFramework:RunNextTest()
end

function TestMT:Complete()
	if (self.Completed) then return end
	self.Completed = true
	MsgC(Color(30, 230, 30), "PASSED\n")
	TestFramework:ReportResult(true)
	TestFramework:RunNextTest()
end

function TestMT:Run()
	MsgC("Test: ", self.Name, " ")
	self.Completed = false
	local status, err = pcall(function()
		self.TestFunction(self)
	end)
	if (!status) then
		self:Fail(err)
	end
end

function TestMT:shouldBeNil(a)
	if (a != nil) then
		self:Fail(tostring(a) .. " was expected to be nil, but was not nil")
		error("Assertion failed")
	end
end

function TestMT:shouldBeGreaterThan(a, num)
	if (num >= a) then
		self:Fail(tostring(a) .. " was expected to be greater than " .. tostring(num))
		error("Assertion failed")
	end
end

function TestMT:shouldNotBeNil(a)
	if (a == nil) then
		self:Fail(tostring(a) .. " was expected to not be nil, but was nil")
		error("Assertion failed")
	end
end

function TestMT:shouldNotBeEqual(a, b)
	if (a == b) then
		self:Fail(tostring(a) .. " was equal to " .. tostring(b))
		error("Assertion failed")
	end
end

function TestMT:shouldBeEqual(a, b)
	if (a != b) then
		self:Fail(tostring(a) .. " was not equal to " .. tostring(b))
		error("Assertion failed")
	end
end

function TestMT:shouldHaveLength(tbl, exactLength)
	if (#tbl != exactLength) then
		self:Fail("Length of " .. tostring(tbl) .. " was not equal to " .. exactLength)
		error("Assertion failed")
	end
end

concommand.Add("mysqloo_start_tests", function(ply)
	if (IsValid(ply)) then return end
	print("Starting MySQLOO Tests")
	if (#player.GetBots() == 0) then
		RunConsoleCommand("bot")
	end
	timer.Simple(0.1, function()
		TestFramework:Start()
	end)
end)