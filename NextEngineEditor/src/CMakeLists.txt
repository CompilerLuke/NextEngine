add_subdirectory(assets)
add_subdirectory(terrain_tools)

target_sources(next_editor PUBLIC
        custom_inspect.cpp
        diffUtil.cpp
        displayComponents.cpp
        editor.cpp
        generated.cpp
        gizmo.cpp
        grass_placement.cpp
        lister.cpp
        objectGizmo.cpp
        picking.cpp
        visualize_physics.cpp
        visualize_profiler.cpp)