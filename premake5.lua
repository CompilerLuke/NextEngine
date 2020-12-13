-- premake5.lua

require "premake_extension"

workspace "NextEngine"
	architecture "x86_64"
	startproject "TheUnpluggingRunner"

	configurations {
		"Debug",
		"Release",
		"Dist"
	}

	flags {
		"MultiProcessorCompile"
	}

	runtime "Release"
	defines "_CRT_SECURE_NO_WARNINGS"

	filter "system:windows"
		staticruntime "Off"
		systemversion "latest"

    filter "system:macosx"
        systemversion "10.15.5"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
freetype = false

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
            unitybuild "on"

    		defines {
    			"%{prj.name:upper()}_EXPORTS",
    			"NE_PLATFORM_WINDOWS"
    		}

    filter "system:macosx"
        defines "NE_PLATFORM_MACOSX"

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
	        "%{prj.name:upper()}_DLL",
	        "%{prj.name:upper()}_EXPORTS"
        }

end

project "NextCore"
	location "NextCore"
	dll_config()


project "ReflectionTool"
	location "ReflectionTool"
	kind "ConsoleApp"

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
		postbuildcommands {
			"{COPY} ../bin/" .. outputdir .. "/NextCore/NextCore.dll ../bin/" .. outputdir .. "/%{prj.name}",
        }
	    reflection_exe = '"../bin/' .. outputdir .. '/ReflectionTool/ReflectionTool.exe"'
    end

	links 
	{
		"NextCore",
	}

	-- $(SolutionDir)x64\Release\ReflectionTool.exe -b $(ProjectDir) lister.h -d assets -o src\generatedts/terrain.h components/lights.h components/grass.h graphics\rhi\forward.h engine/handle.h physics/physics.h -h engine/types -c ecs/component_ids.h -o src/generated -l ENGINE_API

	default_config()
	set_rpath()

VULKAN_SDK = os.getenv("VULKAN_SDK")

project "NextEngine"
	location "NextEngine"

	files {
		"%{prj.name}/vendor/spirv-reflect/*.cpp",
		"%{prj.name}/vendor/spirv-reflect/*.h",
		"%{prj.name}/vendor/imgui/*.cpp",
		"%{prj.name}/vendor/imgui/*.h",
	}

	if freetype then
		files {
			"%{prj.name}/vendor/imgui/misc/freetype/*.cpp",
			"%{prj.name}/vendor/imgui/misc/freetype/*.h",
		}
	end 
	
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

	print(reflection_exe)

	prebuildcommands (reflection_exe .. ' -b "." -i "include" -d graphics/assets -d ecs components/transform.h components/camera.h components/flyover.h components/skybox.h components/terrain.h components/lights.h components/grass.h graphics/rhi/forward.h engine/handle.h physics/physics.h graphics/pass/volumetric.h -h engine/types -c ecs/component_ids.h -o src/generated -l ENGINE_API')

	defines "RENDER_API_VULKAN"
	if freetype then 
	    defines "IMGUI_FREETYPE"
	end 

	dll_config()
	
	filter { "files:NextEngine/vendor/**" }
	    optimize "Full"
		dontincludeinunity "On"

    filter "system:macosx"
        sysincludedirs "/usr/local/Cellar/freetype/2.10.2/include/freetype2"
        libdirs {
            VULKAN_SDK .. "/lib/",
        }

		if freetype then 
			libdirs { "/usr/local/Cellar/freetype/2.10.2/lib" } --maybe it is better to include freetype through git submodules
		end 

        links {"shaderc_shared", "freetype" }
        linkoptions {"-framework Foundation", "-framework Cocoa", "-framework IOKit"}

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

project "ChemistryProject"
	location "ChemistryProject"

	files {
	    "%{prj.name}/include/generated.h",
	    "%{prj.name}/include/chemistry_component_ids.h",
	    "%{prj.name}/src/generated.cpp"
	}

	includedirs { "NextEngine/include", "NextCore/include" }
	links { "NextCore", "NextEngine" }

	prebuildcommands (reflection_exe .. ' -b "." -i "include" rotating.h gold_foil.h emission.h -c "chemistry_component_ids.h" -o src/generated')

	-- $(SolutionDir)x64\Release\ReflectionTool.exe  -b $(ProjectDir) -i "" -d "" -o generated
	dll_config()

project "CFD"
	location "CFD"

	files {
	    "%{prj.name}/include/generated.h",
	    "%{prj.name}/include/cfd_ids.h",
	    "%{prj.name}/src/generated.cpp"
	}

	includedirs { "NextEngine/include", "NextCore/include" }
	links { "NextCore", "NextEngine" }

	prebuildcommands (reflection_exe .. ' -b "." -i "include" components.h -c "cfd_ids.h" -o src/generated')

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
	kind "ConsoleApp"

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

	defines ('NE_BUILD_DIR="' .. outputdir .. '"')

	--$(SolutionDir)x64\Release\ReflectionTool.exe -b $(ProjectDir) lister.h -d assets -o src\generatedts/terrain.h components/lights.h components/grass.h graphics\rhi\forward.h engine/handle.h physics/physics.h -h engine/types -c ecs/component_ids.h -o src/generated -l ENGINE_API

    pch()
	default_config()

group "Dependencies"
	include "NextEngine/vendor/assimp"
	include "NextEngine/vendor/glfw"
    include "NextEngine/vendor/bullet3"