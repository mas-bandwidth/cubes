solution "cubes"
    includedirs { ".", "vectorial" }
    platforms { "x64" }
    configurations { "Debug", "Release" }
    flags { "Symbols", "ExtraWarnings", "EnableSSE2", "FloatFast" , "NoRTTI", "NoExceptions" }
    configuration "Release"
        flags { "OptimizeSpeed" }
        defines { "NDEBUG" }

project "server"
    language "C++"
    buildoptions "-std=c++11"
    kind "ConsoleApp"
    files { "*.cpp" }
    excludes { "client.cpp", "render.cpp" }
    links { "ode", "pthread" }
    defines { "SERVER" }

project "client"
    language "C++"
    buildoptions "-std=c++11 -stdlib=libc++ -Wno-deprecated-declarations"
    kind "ConsoleApp"
    files { "*.cpp" }
    excludes { "server.cpp" }
    links { "ode", "glew", "glfw3", "GLUT.framework", "OpenGL.framework", "Cocoa.framework", "CoreVideo.framework", "IOKit.framework" }
    defines { "CLIENT" }

if _ACTION == "clean" then
    os.remove "client"
    os.remove "server"
    os.rmdir "obj"
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
        trigger     = "client",
        description = "Build and run client",
        valid_kinds = premake.action.get("gmake").valid_kinds,
        valid_languages = premake.action.get("gmake").valid_languages,
        valid_tools = premake.action.get("gmake").valid_tools,
     
        execute = function ()
            if os.execute "make client" == 0 then
                os.execute "./client"
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
            if os.execute "make server" == 0 then
                os.execute "./server"
            end
        end
    }

end
