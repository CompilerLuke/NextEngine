#include "engine/application.h"
#include "engine/engine.h"
#include "engine/input.h"
#include "physics/physics.h"
#include "ecs/ecs.h"
#include "graphics/rhi/window.h"
#include "components/transform.h"
#include "components/flyover.h"
#include "rotating.h"
#include "gold_foil.h"
#include "emission.h"
#include "chemistry_component_ids.h"
#include "graphics/renderer/frame.h"

struct Demo {

};

void update_rotating(World& world, UpdateCtx& ctx) {
    for (auto [e, trans, rotating] : world.filter<Transform, Rotating>(ctx.layermask)) {
        trans.rotation *= glm::angleAxis(glm::radians(rotating.speed) * (float)ctx.delta_time, glm::vec3(0.0f, 1.0f, 0.0f));
    }
}


APPLICATION_API Demo* init(Modules& engine) {
    return new Demo{};
}

APPLICATION_API bool is_running(Demo& game, Modules& modules) {
    return !modules.window->should_close();
}

APPLICATION_API void update(Demo& game, Modules& modules) {
    World& world = *modules.world;
    UpdateCtx ctx(*modules.time, *modules.input);

    update_local_transforms(world, ctx);
    update_flyover(world, ctx);
    update_rotating(world, ctx);
    update_alpha_emitter(world, ctx);
    update_electrons(world, ctx);
    //modules.physics_system->update(world, ctx);
}

#include "graphics/renderer/frame.h"
#include "graphics/renderer/renderer.h"

APPLICATION_API void extract_render_data(Demo& game, Modules& engine, FrameData& data) {
    Renderer& renderer = *engine.renderer;

    extract_alpha_emitter_render_data(*engine.world, data.light_ubo, EntityQuery());

}

APPLICATION_API void render(Demo& game, Modules& engine, GPUSubmission& gpu_submission, FrameData& data) {
    Renderer& renderer = *engine.renderer;

    render_alpha_emitter(*engine.world, gpu_submission.render_passes[0], EntityQuery());

}

APPLICATION_API void deinit(Demo* game, Modules& engine) {
    delete game;
}