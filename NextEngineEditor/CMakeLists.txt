add_library(next_editor SHARED)

add_subdirectory(src)

target_include_directories(next_editor PUBLIC ${PROJECT_SOURCE_DIR}/NextEngineEditor/include)
target_include_directories(next_editor PUBLIC ${PROJECT_SOURCE_DIR}/NextEngineEditor/vendor)
target_link_libraries(next_editor PUBLIC next_core next_engine next_core)

set(NEXT_EDITOR_LIB $<TARGET_FILE_NAME:next_editor>)
message(NEXT_EDITOR_LIB ${NEXT_EDITOR_LIB})

target_sources(next_editor PUBLIC vendor/ImGuizmo/ImGuizmo.cpp vendor/ImGuizmo/ImSequencer.cpp)

IF (WIN32)
    target_compile_definitions(next_core PUBLIC NE_PLATFORM_WINDOWS)
ELSE()
    target_compile_definitions(next_core PUBLIC NE_PLATFORM_MACOSX)
ENDIF()
