add_library(next_ui STATIC)
add_subdirectory(src)

set(FREETYPE_CUSTOM_DIR "" CACHE STRING "FREETYPE_CUSTOM_DIR")
message(FREETYPE_CUSTOM_DIR ${FREETYPE_CUSTOM_DIR})

if("${FREETYPE_CUSTOM_DIR}" STREQUAL "")
    find_package(Freetype REQUIRED)
else()
    find_package(Freetype REQUIRED PATHS ${FREETYPE_CUSTOM_DIR})
endif()

message(FREETYPE_INCLUDE_DIRS ${FREETYPE_INCLUDE_DIRS})

target_include_directories(next_ui PUBLIC ${FREETYPE_INCLUDE_DIRS})
target_include_directories(next_ui PUBLIC ${PROJECT_SOURCE_DIR}/NextEngine/include)
target_include_directories(next_ui PUBLIC ${PROJECT_SOURCE_DIR}/NextEngine/include)
target_include_directories(next_ui PUBLIC ${PROJECT_SOURCE_DIR}/NextUI/include)

target_link_libraries(next_ui PUBLIC next_core next_engine ${FREETYPE_LIBRARIES})


