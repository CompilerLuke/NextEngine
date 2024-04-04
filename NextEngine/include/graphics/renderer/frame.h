#pragma once

#include "ecs/id.h"
#include "graphics/pass/pass.h"
#include "core/container/hash_map.h"
#include "core/container/tvector.h"
#include "core/container/array.h"

class RenderPrepareCtx {
    World& world;
    EntityQuery layermask;
    EntityQuery camera;
    Viewport viewport;
    uint32_t frame;
    RenderPass::ID pass;
public:
    RenderPrepareCtx(World& world, Viewport viewport, EntityQuery layermask, EntityQuery camera, RenderPass::ID pass,
    uint32_t frame);
    uint32_t get_frame();
    Viewport& get_viewport();
    World& get_world();
    EntityQuery get_layermask();
    RenderPass::ID get_pass();
    EntityQuery get_camera_layermask(); // todo: better method of selecting camera
};

class RenderSubmitCtx {
    uint32_t frame;
    hash_map<RenderPass::ID, RenderPass, 100> active_passes; // todo: remove size limitation
    array<16, RenderPass::ID> pass_stack;
public:
    RenderSubmitCtx(uint32_t frame, RenderPass screen, RenderPass::ID current);
    uint32_t get_frame();
    void push_render_pass(RenderPass::ID);
    void pop_render_pass();
    RenderPass get_current_pass();
    RenderPass get_render_pass(RenderPass::ID);
    tvector<RenderPass> get_active_passes();
};

class RenderSystem {
public:
    virtual void extract_render_data(RenderPrepareCtx& ctx) = 0;
    virtual void build_command_buffers(RenderSubmitCtx& ctx) = 0;
    virtual ~RenderSystem();
};
