solution "MySQLOO"

	language "C++"
	location ( "solutions/" .. os.get() .. "-" .. _ACTION )
	flags { "Symbols", "NoEditAndContinue", "NoPCH", "EnableSSE", "NoImportLib"}
	targetdir ( "out/" .. os.get() .. "/" )
	includedirs {	"MySQLOO/include/",
					"GmodLUA/include/",
					"Boost/",
					"MySQL/include/"	 }

	if os.get() == "linux" then

		buildoptions{ "-std=c++11 -fPIC" }
		linkoptions{ "-fPIC -static-libstdc++" }

	end
	
	configurations
	{ 
		"Release"
	}
	platforms
	{
		"x32"
	}
	
	configuration "Release"
		defines { "NDEBUG" }
		flags{ "Optimize", "FloatFast" }

		if os.get() == "windows" then

			defines{ "WIN32" }

		elseif os.get() == "linux" then

			defines{ "LINUX" }

		end
	project "MySQLOO"
		defines{ "GMMODULE" }
		files{ "MySQLOO/source/**.*", "MySQLOO/include/**.*" }
		kind "SharedLib"
		libdirs { "MySQL/lib/" .. os.get() }
		local platform = (os.get() == "linux" and "linux" or "win32")
		targetname( "gmsv_mysqloo_" .. platform)
		targetprefix ("")
		targetextension (".dll")
		targetdir("out/" .. os.get())
		
		if os.get() == "windows" then
			links { "libmysql" }
		elseif os.get() == "linux" then
			links { "mysqlclient" }
		end
		
