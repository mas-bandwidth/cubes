solution "theproduct"
    includedirs { ".", "vectorial" }
    platforms { "x64" }
    configurations { "Debug", "Release" }
    flags { "Symbols", "ExtraWarnings", "EnableSSE2", "FloatFast" , "NoRTTI", "NoExceptions" }
    configuration "Release"
        flags { "OptimizeSpeed" }
        defines { "NDEBUG" }

project "server"
    language "C++"
    buildoptions "-std=c++11 -stdlib=libc++ -Wno-deprecated-declarations"
    kind "ConsoleApp"
    files { "*.cpp" }
    excludes { "client.cpp" }
    location "build"
    targetdir "bin"
    defines { "SERVER" }

project "client"
    language "C++"
    buildoptions "-std=c++11 -stdlib=libc++ -Wno-deprecated-declarations"
    kind "ConsoleApp"
    files { "*.cpp" }
    excludes { "server.cpp" }
    links { "glew", "glfw3", "GLUT.framework", "OpenGL.framework", "Cocoa.framework", "CoreVideo.framework", "IOKit.framework" }
    location "build"
    targetdir "bin"
    defines { "CLIENT" }

if _ACTION == "clean" then
    os.rmdir "bin"
    os.rmdir "obj"
    os.rmdir "build"
    if not os.is "windows" then
        os.execute "rm -f *.zip"
        os.execute "rm -f *.txt"
        os.execute "rm -rf output"
        os.execute "find . -name *.DS_Store -type f -exec rm {} \\;"
    else
        os.rmdir "ipch"
        os.execute "del /F /Q *.zip"
    end
end

if not os.is "windows" then

    newaction 
    {
        trigger     = "loc",
        description = "Count lines of code",
        valid_kinds = premake.action.get("gmake").valid_kinds,
        valid_languages = premake.action.get("gmake").valid_languages,
        valid_tools = premake.action.get("gmake").valid_tools,

        execute = function ()
            os.execute "find . -name *.h -o -name *.cpp | xargs wc -l"
        end
    }

    newaction
    {
        trigger     = "zip",
        description = "Zip up archive of this project",
        valid_kinds = premake.action.get("gmake").valid_kinds,
        valid_languages = premake.action.get("gmake").valid_languages,
        valid_tools = premake.action.get("gmake").valid_tools,
     
        execute = function ()
            _ACTION = "clean"
            premake.action.call( "clean" )
            os.execute "zip -9r theproduct.zip *"
        end
    }

    newaction
    {
        trigger     = "client",
        description = "Build and run client",
        valid_kinds = premake.action.get("gmake").valid_kinds,
        valid_languages = premake.action.get("gmake").valid_languages,
        valid_tools = premake.action.get("gmake").valid_tools,
     
        execute = function ()
            if os.execute "make -j4 client" == 0 then
                os.execute "bin/client"
            end
        end
    }

    newaction
    {
        trigger     = "server",
        description = "Build and run server",
        valid_kinds = premake.action.get("gmake").valid_kinds,
        valid_languages = premake.action.get("gmake").valid_languages,
        valid_tools = premake.action.get("gmake").valid_tools,
     
        execute = function ()
            if os.execute "make -j4 server" == 0 then
                os.execute "bin/server"
            end
        end
    }

end
