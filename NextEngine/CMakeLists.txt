add_library(next_engine STATIC include/next_engine.h)
target_include_directories(next_engine PUBLIC include)
target_include_directories(next_engine PUBLIC ${PROJECT_SOURCE_DIR}/NextCore/include)
target_include_directories(next_engine PUBLIC ${PROJECT_SOURCE_DIR}/vendor/glm)

set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

target_include_directories(next_engine PUBLIC ${Vulkan_INCLUDE_DIRS})

target_include_directories(next_engine PUBLIC ${PROJECT_SOURCE_DIR}/NextEngine/vendor/glfw/include)
target_include_directories(next_engine PUBLIC ${PROJECT_SOURCE_DIR}/NextEngine/vendor)
add_subdirectory(src)

add_subdirectory(vendor/glfw)
add_subdirectory(vendor/assimp)
add_subdirectory(vendor/spirv-reflect)
add_subdirectory(vendor/imgui)

message(ShaderC ${Vulkan_INCLUDE_DIRS} -------)
target_link_libraries(next_engine PUBLIC next_core glm glfw assimp Vulkan::shaderc_combined)

target_compile_definitions(next_engine PUBLIC RENDER_API_VULKAN)

IF (WIN32)
    target_compile_definitions(next_core PUBLIC NE_PLATFORM_WINDOWS)
ELSE()
    target_compile_definitions(next_core PUBLIC NE_PLATFORM_MACOSX)
ENDIF()

