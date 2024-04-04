add_library(next_core STATIC)
add_subdirectory(src)
target_include_directories(next_core PUBLIC ${PROJECT_SOURCE_DIR}/NextCore/include)
target_include_directories(next_core PUBLIC ${PROJECT_SOURCE_DIR}/vendor/glm)
target_link_libraries(next_core glm)

target_compile_definitions(next_core PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)

IF (WIN32)
    target_compile_definitions(next_core PUBLIC NE_PLATFORM_WINDOWS)
ELSE()
    target_compile_definitions(next_core PUBLIC NE_PLATFORM_MACOSX)
ENDIF()
