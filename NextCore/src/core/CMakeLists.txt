add_subdirectory(container)
add_subdirectory(io)
add_subdirectory(job_system)
add_subdirectory(memory)

target_sources(next_core PUBLIC profiler.cpp reflection.cpp serializer.cpp time.cpp)