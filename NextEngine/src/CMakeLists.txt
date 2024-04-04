add_subdirectory(components)
add_subdirectory(ecs)
add_subdirectory(engine)
add_subdirectory(graphics)
#add_subdirectory(physics)

target_sources(next_engine PUBLIC generated.cpp)