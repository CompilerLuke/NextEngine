-- premake5.lua

require "premake_extension"

workspace "NextEngine"
	architecture "x64"
	startproject "TheUnpluggingRunner"

	configurations {
		"Debug",
		"Release",
		"Dist"
	}

	flags {
		"MultiProcessorCompile"
	}

	filter "system:windows"
		staticruntime "Off"
		systemversion "latest"

    filter "system:macosx"
        systemversion "10.15.5"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

function pch()
	pchheader "stdafx.h"
	pchsource "%{prj.name}/stdafx.cpp"

	includedirs { "%{prj.name}/" }

	files {
		"%{prj.name}/stdafx.h",
		"%{prj.name}/stdafx.cpp"
	}
end

function copy_into_runner()
    filter "*"
        postbuildcommands {
            ("{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/TheUnpluggingRunner")
        }
end

function set_rpath()
    if os.istarget("macosx") then
        filter "*"
            postbuildcommands {
                "install_name_tool -change /usr/local/lib/libNextCore.dylib @executable_path/libNextCore.dylib ../bin/" .. outputdir .. "/%{prj.name}/%{prj.name}"
            }
    end
end

function default_config()
    language "C++"
    cppdialect "C++17"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files {
        "%{prj.name}/include/**.h",
        "%{prj.name}/src/**.cpp"
    }

    includedirs {
        "%{prj.name}/include",
        "%{prj.name}/"
    }
    sysincludedirs "vendor/glm"

    filter "system:windows"
    		staticruntime "Off"
    		runtime "Release"
            unitybuild "on"

    		defines {
    			"%{prj.name:upper()}_EXPORTS",
    			"NE_PLATFORM_WINDOWS"
    		}

    filter "system:macosx"
        defines "NE_MACOSX"

    filter "configurations:Debug"
        defines "NE_DEBUG"
        symbols "On"

    filter "configurations:Release"
        defines "NE_RELEASE"
        symbols "On"
        optimize "On"

    filter "configurations:Dist"
        defines "NE_DIST"
        optimize "On"
end


function dll_config()
    kind "SharedLib"
	pch()
	default_config()
    copy_into_runner()

	filter "system:windows"
	    defines {
	        "%{prj.name.upper()}_DLL",
	        "%{prj.name:upper()}_EXPORTS"
        }

end

project "NextCore"
	location "NextCore"
	dll_config()


VULKAN_SDK = os.getenv("VULKAN_SDK")

project "NextEngine"
	location "NextEngine"

	files {
		"%{prj.name}/vendor/spirv-reflect/*.cpp",
		"%{prj.name}/vendor/spirv-reflect/*.h",
		"%{prj.name}/vendor/imgui/**.cpp",
		"%{prj.name}/vendor/imgui/**.h",
	}
	
	includedirs {
		"NextCore/include"
	}

	sysincludedirs {
        VULKAN_SDK .. "/Include",
        "%{prj.name}/vendor/",
        "%{prj.name}/vendor/glfw/include",
        "%{prj.name}/vendor/assimp/include",
        "%{prj.name}/vendor/bullet3/src",
	}


	links 
	{
		"NextCore",
		"assimp",
		"glfw",
		"BulletInverseDynamics",
		"BulletSoftBody",
		"BulletDynamics",
		"BulletCollision",
		"LinearMath"
	}


	-- $(SolutionDir)x64\Release\ReflectionTool.exe -b $(ProjectDir) -d graphics/assets -d ecs components/transform.h components/camera.h components/flyover.h components/skybox.h components/terrain.h components/lights.h components/grass.h graphics\rhi\forward.h engine/handle.h physics/physics.h -h engine/types -c ecs/component_ids.h -o src/generated -l ENGINE_API

	defines "RENDER_API_VULKAN"

	dll_config()
	
	filter { "files:NextEngine/vendor/**" }
	    optimize "Full"
		dontincludeinunity "On"

    filter "system:macosx"
        sysincludedirs "/usr/local/Cellar/freetype/2.10.2/include/freetype2"
        libdirs {
            VULKAN_SDK .. "/lib/",
            "/usr/local/Cellar/freetype/2.10.2/lib" --maybe it is better to include freetype through git submodules
        }
        links {"shaderc_shared", "freetype" }
        linkoptions {"-framework Foundation", "-framework Cocoa", "-framework IOKit"}
        defines "IMGUI_FREETYPE"

    filter "system:windows"
        links "%{VULKAN_SDK}/lib/shaderc_combined.lib"


project "NextEngineEditor"
	location "NextEngineEditor"

	files 
	{
	    "%{prj.name}/vendor/ImGuizmo/*.h",
		"%{prj.name}/vendor/ImGuizmo/*.cpp"
	}
	
	includedirs {
		"NextEngine/include",
		"NextCore/include",
	}

	sysincludedirs {
        "vendor/glm",
        "NextEngine/vendor/",
        "NextEngineEditor/vendor/",
        "%{VULKAN_SDK}/Include",
	}

	links 
	{
		"NextCore",
		"NextEngine",
	}

	-- $(SolutionDir)x64\Release\ReflectionTool.exe -b $(ProjectDir) lister.h -d assets -o src\generatedts/terrain.h components/lights.h components/grass.h graphics\rhi\forward.h engine/handle.h physics/physics.h -h engine/types -c ecs/component_ids.h -o src/generated -l ENGINE_API
	defines "RENDER_API_VULKAN"
	dll_config()

	filter { "files:NextEngineEditor/vendor/**" }
	    optimize "Full"
		dontincludeinunity "On"

project "ReflectionTool"
	location "ReflectionTool"
	kind "ConsoleApp"
	entrypoint "main"

	includedirs {
		"NextCore/include",
		"vendor/glm",
	}

	if os.istarget("macosx") then
	    postbuildcommands {
	        "cp ../bin/" .. outputdir .. "/NextCore/libNextCore.dylib ../bin/" .. outputdir .. "/%{prj.name}/libNextCore.dylib",
        }
	    reflection_exe = "../bin/" .. outputdir .. "/ReflectionTool/ReflectionTool"
	else
	    reflection_exe = "../bin/" .. outputdir .. "/ReflectionTool/ReflectionTool.exe"
    end

	links 
	{
		"NextCore",
	}

	-- $(SolutionDir)x64\Release\ReflectionTool.exe -b $(ProjectDir) lister.h -d assets -o src\generatedts/terrain.h components/lights.h components/grass.h graphics\rhi\forward.h engine/handle.h physics/physics.h -h engine/types -c ecs/component_ids.h -o src/generated -l ENGINE_API

	default_config()
	set_rpath()

project "ChemistryProject"
	location "ChemistryProject"

	files {
	    "%{prj.name}/include/generated.h",
	    "%{prj.name}/include/chemistry_component_ids.h",
	    "%{prj.name}/src/generated.cpp"
	}

	includedirs { "NextEngine/include", "NextCore/include" }
	links { "NextCore", "NextEngine" }

	prebuildcommands (reflection_exe .. ' -b "." -i "include" -d "" -c "chemistry_component_ids.h" -o src/generated')

	-- $(SolutionDir)x64\Release\ReflectionTool.exe  -b $(ProjectDir) -i "" -d "" -o generated
	dll_config()

project "TheUnpluggingGame"
	location "TheUnpluggingGame"

	files {
	    "%{prj.name}/*.h",
	    "%{prj.name}/*.cpp",
	    "%{prj.name}/generated.cpp",
	}

	includedirs {
        "NextEngine/include",
        "NextCore/include",
    }
	
	sysincludedirs {
		"NextEngine/include", --todo could always use "" include and not <>
		"NextCore/include",
	}

	links 
	{
		"NextCore",
		"NextEngine"
	}

	prebuildcommands {
	    reflection_exe .. ' -b "." -i "" -d "" -o generated'
	}

	-- $(SolutionDir)x64\Release\ReflectionTool.exe  -b $(ProjectDir) -i "" -d "" -o generated
	dll_config()

project "TheUnpluggingRunner"
	location "TheUnpluggingRunner" 
	kind "WindowedApp"
	entrypoint "main"

	includedirs {
        "NextEngine/include",
        "NextCore/include",
	}
	
	sysincludedirs {
		"NextEngine/include",
		"NextCore/include",
	}

	links 
	{
		"NextCore",
		"NextEngine",
	}

	--$(SolutionDir)x64\Release\ReflectionTool.exe -b $(ProjectDir) lister.h -d assets -o src\generatedts/terrain.h components/lights.h components/grass.h graphics\rhi\forward.h engine/handle.h physics/physics.h -h engine/types -c ecs/component_ids.h -o src/generated -l ENGINE_API

    pch()
	default_config()


group "Dependencies"
	include "NextEngine/vendor/assimp"
	include "NextEngine/vendor/glfw"
    include "NextEngine/vendor/bullet3"