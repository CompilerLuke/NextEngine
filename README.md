# NextEngine
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/56322335a66b4e599ea0ad0ba46a2bc9)](https://app.codacy.com/manual/CompilerLuke/NextEngine?utm_source=github.com&utm_medium=referral&utm_content=CompilerLuke/NextEngine&utm_campaign=Badge_Grade_Settings)

NextEngine is designed with rapid iteration and high-end desktop graphics in mind. The engine has been fundamentally redesigned in a data-oriented fashion, with efforts to parallize the engine under-way. The engine, ported from OpenGL is vulkan-first and is cross-platform, with the capability of running on Windows, MacOSX and theoretically Linux, though this functionality has not been tested. 

# Friendly warning
This being a hobby project, implemented solely by one person, the mentality has been to move fast and break things. Though certain features of the engine are relatively mature, production level stability should not be expected or relied upon! Moreover development of new features will heavily depend on my spare time, as such there may be periods of massive improvements+changes and others of relative stagnation.

# Getting Started Guide

1. Clone the project
```console
git clone --recurse-submodules https://github.com/CompilerLuke/NextEngine.git
cd NextEngine
```
2. Generate appropriate solution/project
Enviroment Variable VULKAN_SDK must be set and point to a version 1.2 of the Vulkan SDK or higher

On MacOSX
```console
./vendor/premake/premake5 xcode4
```
On Windows
```console
vendor/premake/premake5.exe vs2017
```

3. Open generated solution
Modify paths in TheUnpluggingRunner/main.cpp. 

4. Compile all + Run
Hopefully everything is working as expected and a window with the editor should popup :)

# Skeleton
To allow for live-code reloads your game must be built as a DLL with the following function definitions exported. These serve as the basic interface between the engine and the game, Modules& contains pointers to all instantiated systems. There are few restrictions on the application with almost all of the engine's internal apis being public and calleable from the DLL. In fact the editor is an application itself and not part of the core engine. Recompiling your code should automatically trigger the DLL to be reloaded and the code to updated. Any changes to components will also be reflected in the editor. Word of caution with live-coding, changing structures other than Components will likely result in a crash. 

```c++
#include "engine/application.h"
#include "engine/engine.h"
#include "myproject_generated.h" -- Generated header from reflection system, includes component definitions

struct Game {};

APPLICATION_API Game* init(Modules& engine) {
    return new Game{};
}

APPLICATION_API bool is_running(Game& game, Modules& modules) {
    return !modules.window->should_close();
}

APPLICATION_API void update(Game& game, Modules& modules) {
    World& world = *modules.world;
    UpdateCtx ctx(*modules.time, *modules.input);

    update_local_transforms(world, ctx);
    // -- Insert your game's update loop
    modules.physics_system->update(world, ctx);
}

APPLICATION_API void deinit(Game* game, Modules& engine) {
    delete game;
}


//ADVANCED - custom rendering the RHI
APPLICATION_API void extract_render_data(Game& game, Modules& engine, FrameData& data) {
    
}

APPLICATION_API void render(Demo& game, Modules& engine, GPUSubmission& gpu_submission, FrameData& data) {
    
}

```

# Examples
Check out either the TheUnpluggingGame or ChemistryProject folder.

# Features
1. PBR Image Based Shading
2. Archetype ECS
3. Built-in editor
4. Live code reloading + Adding/removing fields on entities
5. Basic physics support
6. Terrain rendering 
7. Kriging based heightmap generation and splat rendering
8. Grass and vegetation rendering and planting system
9. Point lights, 
10. Shader graph
11. Lister panel and nested entities
12. Asset tab, filedialog import + drag and drop
13. BVH-Culling and optimizations for static meshes
14. AABB based in-editor picking
15. Redo-undo system
16. Profiler
17. Fiber-based job system
18. Built in, performant collections
19. Compile time reflection/serialization based on header parser
20. Simple frame-graph system which eliminates renderpass/framebuffer boilerplate

# Goal
This project serves as a test-bed for new ideas and developing my skills in computer graphics and low-level programming. The code is written in a variety of styles, ranging from OOP, with templated meta-programming thrown in, to low-level procedural routines manipulating memory at the byte level. As such the project is instrumental in determing the merit of various coding styles and best-practices, to filter out those which suit me the best. In terms of feature-set the engine is relatively far along and can be used for simple games/simulations. I plan on improving the lighting by adding spot-light support, global illumination as well as porting over the cascaded shadow map and volumetric lighting present in previous versions to vulkan. Moreover prefab support is currently missing and the physics system needs a little bit more love. Support for skeletal animation will be implemented soon. 

# Screenshots

![Shader Graph support](https://media.discordapp.net/attachments/490868844760530944/768536285861380168/shadergraph_demo.JPG?width=2688&height=776)
Shader Graph

![MacOSX Chemistry Simulation](https://cdn.discordapp.com/attachments/490868844760530944/774741962640064532/Screenshot_2020-11-07_at_22.07.05.png)
MacOSX Chemistry Simulation

![MacOSX Chemistry Simulation](https://cdn.discordapp.com/attachments/490868844760530944/774742011923398656/Screenshot_2020-11-07_at_22.03.36.png)

![Normal Mapping](https://media.discordapp.net/attachments/490868844760530944/726342993606475816/Wood.PNG)
Normal Mapping

![Material Editor](https://media.discordapp.net/attachments/490868844760530944/725715187125977119/cubemap.PNG?width=2684&height=1351)
Material Editor

![BVH Visualization](https://media.discordapp.net/attachments/490868844760530944/690621959016546344/algo.PNG)
BVH Visualization

![Kriging control-point heightmap interpolation](https://media.discordapp.net/attachments/490868844760530944/731453786064683079/kriging.PNG)
Kriging control-point heightmap interpolation

![LOD Grass Rendering](https://media.discordapp.net/attachments/490868844760530944/737713178166820945/lods.JPG?width=2338&height=1349)
LOD Grass Rendering

![Terrain texture splatting + Editor Icons](https://media.discordapp.net/attachments/490868844760530944/770595325739401226/Screenshot_2020-10-27_at_11.25.30.png?width=2308&height=1350)
Terrain texture splatting + Editor Icons

![Volumetric Lighting + Shadows + Fog (currently not supported)](https://media.discordapp.net/attachments/490868844760530944/683654920368554017/TreesAnime.PNG)
Volumetric Lighting + Shadows + Fog (currently not supported)


