#pragma once

#include "ecs/id.h"

struct CommandBuffer;
struct World;

struct PhysicsResources {
    material_handle line;
    material_handle solid;
};

void make_physics_resources(PhysicsResources& resources);

void destroy_physics_resources();

void render_colliders(PhysicsResources& resources, CommandBuffer& cmd_buffer, World& world, EntityQuery query);
