function os.winSdkVersion()
	local reg_arch = iif( os.is64bit(), "\\Wow6432Node\\", "\\" )
	local sdk_version = os.getWindowsRegistry( "HKLM:SOFTWARE" .. reg_arch .."Microsoft\\Microsoft SDKs\\Windows\\v10.0\\ProductVersion" )
	if sdk_version ~= nil then return sdk_version end
end

solution "MySQLOO"
	language "C++"
	location ( "solutions/" .. os.target() .. "-" .. _ACTION )
	flags { "NoPCH", "NoImportLib"}
	targetdir ( "out/" .. os.target() .. "/" )
	includedirs {	"MySQLOO/include/",
					"GmodLUA/include/",
					"Boost/",
					"MySQL/include/"	 }
	if os.target() == "macosx" or os.target() == "linux" then
		buildoptions{ "-std=c++11 -fPIC" }
		linkoptions{ "-fPIC -static-libstdc++" }
    end

	configurations { "Release" }
	platforms { "x86", "x86_64" }

	if os.target() == "windows" then
		defines{ "WIN32" }
	elseif os.target() == "linux" then
		defines{ "LINUX" }
	end

	local platform
	if os.target() == "windows" then
		platform = "win"
	elseif os.target() == "macosx" then
		platform = "osx"
	elseif os.target() == "linux" then
		platform = "linux"
	else
		error "Unsupported platform."
	end

	filter "platforms:x86"
		architecture "x86"
		libdirs { "MySQL/lib/" .. os.target() }
		if os.target() == "windows" then
			targetname( "gmsv_mysqloo_" .. platform .. "32")
		else
			targetname( "gmsv_mysqloo_" .. platform)
		end
	filter "platforms:x86_64"
		architecture "x86_64"		
		libdirs { "MySQL/lib64/" .. os.target() }
		targetname( "gmsv_mysqloo_" .. platform .. "64")
	filter {"system:windows", "action:vs*"}
		systemversion((os.winSdkVersion() or "10.0.16299") .. ".0")
		toolset "v141"

	project "MySQLOO"
		symbols "On"
		editandcontinue "Off"
		vectorextensions "SSE"
		floatingpoint "Fast"
		optimize "On"
		
		defines{ "GMMODULE", "NDEBUG" }
		files{ "MySQLOO/source/**.*", "MySQLOO/include/**.*" }
		kind "SharedLib"
		targetprefix ("")
		targetextension (".dll")
		targetdir("out/" .. os.target())
		
		if os.target() == "windows" then
			links { "mysqlclient", "ws2_32.lib", "shlwapi.lib" }
		elseif os.target() == "macosx" or os.target() == "linux" then
			links { "mysqlclient" }
		end
