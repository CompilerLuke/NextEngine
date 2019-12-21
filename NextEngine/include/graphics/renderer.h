#pragma once

#include "core/vector.h"
#include "renderFeature.h"
#include "renderPass.h"
#include <glm/mat4x4.hpp>
#include "core/handleManager.h"

struct Engine;

struct RenderSettings {
	enum GraphicsQuality { Ultra, High, Medium, Low } quality = Ultra;
};

struct RenderExtension {
	virtual void render_view(struct World&, struct RenderParams&) {};
	virtual void render(struct World&, struct RenderParams&) {};
	virtual ~RenderExtension() {};
};

class ENGINE_API Renderer {
	std::unique_ptr<Pass> main_pass;
	RenderSettings settings;

	struct ModelRendererSystem* model_renderer;
	struct GrassRenderSystem* grass_renderer;
	struct TerrainRenderSystem* terrain_renderer;

public:
	glm::mat4* model_m;

	void set_render_pass(Pass* pass);
	void update_settings(const RenderSettings&);
	
	static void render_view(World& world, RenderParams&);
	static void set_shader_scene_params(World& world, struct RenderParams& params, Handle<Shader> shader, Handle<ShaderConfig> config);
	static void render(World& world, struct RenderParams& params);
};

struct ENGINE_API RenderParams {
	Renderer& renderer;
	RenderExtension* extension;

	Layermask layermask;
	struct CommandBuffer* command_buffer;
	struct Pass* pass;
	struct Skybox* skybox;
	struct DirLight* dir_light;

	glm::mat4 projection;
	glm::mat4 view;
	struct Camera* cam = NULL;

	glm::mat4* model_m;

	unsigned int width = 0;
	unsigned int height = 0;
	void set_shader_scene_params(Handle<struct Shader>, Handle<struct ShaderConfig>, struct World&);

	RenderParams(struct Renderer& renderer, struct CommandBuffer*, struct Pass*);
	RenderParams(Engine& engine, Layermask mask, RenderExtension* ext = NULL);
};

struct ENGINE_API PreRenderParams {
	Renderer& renderer;

	Layermask layermask;

	PreRenderParams(Renderer& renderer, Layermask layermask);
};