#pragma once

struct World;
struct SceneViewport;
struct InputMeshRegistry;
struct CFDDebugRenderer;

void handle_scene_interactions(World&, InputMeshRegistry& registry, SceneViewport&, CFDDebugRenderer&);