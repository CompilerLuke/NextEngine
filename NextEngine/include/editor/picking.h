#pragma once
#include "ecs/id.h"
#include "graphics/shader.h"
#include "ecs/system.h"
#include "graphics/pass.h"
#include "core/handle.h"
#include "ecs/system.h"
#include "graphics/materialSystem.h"
#include "graphics/frameBuffer.h"

struct PickingPass : Pass {
	Handle<struct Shader> picking_shader;
	Framebuffer framebuffer;
	Handle<struct Texture> picking_map;
	
	void set_shader_params(Handle<struct Shader>, Handle<struct ShaderConfig>, struct World&, struct RenderParams&) override;

	int pick(struct World&, struct Input&);
	float pick_depth(struct World&, struct Input&);

	void render(struct World&, struct RenderParams&) override;

	glm::vec2 picking_location(struct Input&);

	PickingPass(struct Window&);
};

struct PickingSystem : System {
	struct Editor& editor;
	Handle<struct Shader> outline_shader;
	Material outline_material;
	DrawCommandState object_state;
	DrawCommandState outline_state;

	PickingSystem(Editor&);

	void render(struct World&, struct RenderParams&) override;
};