project(NextEngine)
set(CMAKE_CXX_STANDARD 17)

set(NE_BUILD_GLM ON)

if(NE_BUILD_GLM)
add_subdirectory(vendor/glm)
endif()

add_subdirectory(NextCore)
add_subdirectory(NextEngine)
add_subdirectory(NextUI)
add_subdirectory(NextEngineEditor)