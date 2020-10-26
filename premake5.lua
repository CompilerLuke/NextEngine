-- premake5.lua

require "premake_extension"

workspace "NextEngine"
	architecture "x64"
	startproject "TheUnpluggingRunner"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	flags
	{
		"MultiProcessorCompile"
	}

	filter "system:windows"
		staticruntime "Off"
		systemversion "latest"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

function pch()
	pchheader "stdafx.h"
	pchsource "%{prj.name}/stdafx.cpp"

	includedirs { "%{prj.name}/" }

	files 
	{
		"%{prj.name}/stdafx.h",
		"%{prj.name}/stdafx.cpp"
	}
end

function dll_config()
	pch()
	cppdialect "C++17"

	filter "system:windows"
		staticruntime "Off"
		runtime "Release"
		systemversion "latest"

		defines 
		{
			"%{prj.name:upper()}_EXPORTS",
			"NE_PLATFORM_WINDOWS"
		}

		postbuildcommands
		{
			("{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/TheUnpluggingRunner")
		}

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

	unitybuild "On"

end 

project "NextCore"
	location "NextCore"
	kind "SharedLib"
	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files 
	{
		"%{prj.name}/include/**.h",
		"%{prj.name}/src/**.cpp"
	}

	includedirs {
		"%{prj.name}/include;",
		"vendor/glm;"
	}

	dll_config()
	

VULKAN_SDK = os.getenv("VULKAN_SDK")

project "NextEngine"
	location "NextEngine"
	kind "SharedLib"

	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	unitybuild "On"

	files 
	{
		"%{VULKAN_SDK}/include",
		"%{prj.name}/include/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/spirv-reflect/*.cpp",
		"%{prj.name}/vendor/spirv-reflect/*.h",
		"%{prj.name}/vendor/imgui/*.cpp",
		"%{prj.name}/vendor/imgui/*.h",
	}
	
	includedirs {
		"%{prj.name}/",
		"%{prj.name}/include;",
		"%{prj.name}/vendor/glfw/include;",
		"%{prj.name}/vendor/assimp/include;",
		"%{prj.name}/vendor/bullet3/src;",
		"vendor/glm;",
		"%{VULKAN_SDK}/Include;",
		"NextCore/include;"
	}

	links 
	{
		"NextCore",
		"assimp",
		"glfw",
		"%{VULKAN_SDK}/lib/shaderc_combined.lib",
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
		dontincludeinunity "On"


project "NextEngineEditor"
	location "NextEngineEditor"
	kind "SharedLib"

	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files 
	{
		"%{prj.name}/include/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/ImGuizmo/*.cpp",
	}
	
	includedirs {
		"%{prj.name}/include;",
		"%{VULKAN_SDK}/Include;",
		"NextEngine/include;",
		"NextCore/include;",
		"vendor/glm;",
		"NextEngine/vendor/"
	}

	links 
	{
		"NextCore",
		"NextEngine",
	}

	unitybuild "On"

	-- $(SolutionDir)x64\Release\ReflectionTool.exe -b $(ProjectDir) lister.h -d assets -o src\generatedts/terrain.h components/lights.h components/grass.h graphics\rhi\forward.h engine/handle.h physics/physics.h -h engine/types -c ecs/component_ids.h -o src/generated -l ENGINE_API
	defines "RENDER_API_VULKAN"
	dll_config()

	filter { "files:NextEngineEditor/vendor/**" }
		dontincludeinunity "On"

project "ReflectionTool"
	location "ReflectionTool"
	kind "ConsoleApp"

	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
	entrypoint ("main")

	files 
	{
		"%{prj.name}/include/**.h",
		"%{prj.name}/src/**.cpp"
	}
	
	includedirs {
		"%{prj.name}/include;",
		"NextCore/include;",
		"vendor/glm",
	}

	links 
	{
		"NextCore",
	}

	-- $(SolutionDir)x64\Release\ReflectionTool.exe -b $(ProjectDir) lister.h -d assets -o src\generatedts/terrain.h components/lights.h components/grass.h graphics\rhi\forward.h engine/handle.h physics/physics.h -h engine/types -c ecs/component_ids.h -o src/generated -l ENGINE_API

	filter "system:windows"
		cppdialect "C++17"
		staticruntime "Off"
		runtime "Release"
		systemversion "latest"

		defines 
		{
			"NE_PLATFORM_WINDOWS"
		}

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

project "TheUnpluggingGame"
	location "TheUnpluggingGame"
	kind "SharedLib"

	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files 
	{
		"%{prj.name}/include/**.h",
		"%{prj.name}/src/**.cpp"
	}
	
	includedirs {
		"%{prj.name}/include;",
		"NextEngine/include;",
		"NextCore/include;",
		"vendor/glm;"
	}

	links 
	{
		"NextCore",
		"NextEngine"
	}

	unitybuild "On"

	-- $(SolutionDir)x64\Release\ReflectionTool.exe  -b $(ProjectDir) -i "" -d "" -o generated

	dll_config()

project "TheUnpluggingRunner"
	location "TheUnpluggingRunner" 
	kind "WindowedApp"
	entrypoint "main"

	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files 
	{
		"%{prj.name}/include/**.h",
		"%{prj.name}/src/**.cpp"
	}
	
	includedirs {
		"NextEngine/include;",
		"NextCore/include;",
		"vendor/glm;"
	}

	links 
	{
		"NextCore",
		"NextEngine",
	}

	-- $(SolutionDir)x64\Release\ReflectionTool.exe -b $(ProjectDir) lister.h -d assets -o src\generatedts/terrain.h components/lights.h components/grass.h graphics\rhi\forward.h engine/handle.h physics/physics.h -h engine/types -c ecs/component_ids.h -o src/generated -l ENGINE_API

	dll_config()

	filter "system:windows"
		links "vcruntime.lib"

group "Dependencies"
	include "NextEngine/vendor/assimp"
	include "NextEngine/vendor/glfw"
	require "NextEngine/vendor/bullet3/src/BulletInverseDynamics/premake4"
	require "NextEngine/vendor/bullet3/src/BulletSoftBody/premake4"
	require "NextEngine/vendor/bullet3/src/BulletDynamics/premake4"
	require "NextEngine/vendor/bullet3/src/BulletCollision/premake4"
	require "NextEngine/vendor/bullet3/src/LinearMath/premake4"

