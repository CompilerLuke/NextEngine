#include "stdafx.h"

#include "graphics/renderer/ibl.h"
#include "graphics/rhi/draw.h"
#include "graphics/renderer/renderer.h"
#include "graphics/assets/assets.h"
#include "ecs/ecs.h"
#include "graphics/rhi/frame_buffer.h"
#include "glad/glad.h"
#include <glm/gtc/matrix_transform.hpp>
#include "graphics/rhi/primitives.h"
#include "components/transform.h"
#include "core/io/logger.h"
#include "core/io/vfs.h"
#include <stb_image.h>
#include "components/camera.h"
#include "graphics/pass/render_pass.h"
#include "graphics/assets/material.h"

REFLECT_STRUCT_BEGIN(Skybox)
REFLECT_STRUCT_MEMBER(filename)
REFLECT_STRUCT_MEMBER(capture_scene)
REFLECT_STRUCT_END()

struct DrawCommandState skybox_draw_state = {
	Cull_None,
	DepthFunc_Lequal,
	false,
	draw_skybox
};

void SkyboxSystem::bind_ibl_params(ShaderConfig& config, RenderCtx& ctx) {
	Skybox* skybox = ctx.skybox;
	auto bind_to = ctx.command_buffer.next_texture_index();
	
	/*gl_bind_cubemap(assets.cubemaps, skybox->irradiance_cubemap, bind_to);
	config.set_int("irradianceMap", bind_to);
	
	bind_to = ctx.command_buffer.next_texture_index();
	gl_bind_cubemap(assets.cubemaps, skybox->prefilter_cubemap, bind_to);
	config.set_int("prefilterMap", bind_to);

	bind_to = ctx.command_buffer.next_texture_index();
	gl_bind_to(assets.textures, skybox->brdf_LUT, bind_to);
	config.set_int("brdfLUT", bind_to);*/
}

struct CubemapCapture {
	RenderCtx* params;
	unsigned int width;
	unsigned int height;
	World* world;
	int captureFBO;
	int captureTexture;
	ID capture_id;

	glm::mat4 captureViews[6];

	CubemapCapture() {
		captureViews[0] = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		captureViews[1] = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		captureViews[2] = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		captureViews[3] = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
		captureViews[4] = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
		captureViews[5] = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
	}

	void capture(unsigned int i) {
		assert(i < 6);

		World& world = *this->world;

		ID main_camera = get_camera(world, GAME_LAYER);

		RenderCtx new_params = *params;
		new_params.width = width;
		new_params.height = height;

		ID id = world.make_ID();
		Entity* e = world.make<Entity>(id);
		Transform* new_trans = world.make<Transform>(id);
		new_trans->position = world.by_id<Transform>(capture_id)->position;
		new_trans->rotation = glm::inverse(glm::quat_cast(captureViews[i])); //captureViewsQ[i];

		Camera* camera = world.make<Camera>(id);
		camera->far_plane = 600;
		camera->near_plane = 0.1f;
		camera->fov = 90.0f;

		new_params.cam = camera;
		new_params.layermask = GAME_LAYER;

		world.by_id<Entity>(main_camera)->enabled = false;

		update_camera_matrices(world, id, new_params);

		((MainPass*)new_params.pass)->render_to_buffer(world, new_params, [this, i]() {
			glViewport(0, 0, width, width); // don't forget to configure the viewport to the capture dimensions.
			glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, captureTexture, 0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		});

		world.free_now_by_id(id);

		world.by_id<Entity>(main_camera)->enabled = true;
	}
};

void SkyboxSystem::load(Skybox* skybox) { //todo cleanup
	if (skybox->brdf_LUT.id != INVALID_HANDLE) return;

#ifdef RENDER_API_OPENGL

	bool take_capture = false;
	
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	Shader* equirectangular_to_cubemap_shader = shaders.get(shaders.load("shaders/eToCubemap.vert", "shaders/eToCubemap.frag"));
	Shader* irradiance_shader = shaders.get(shaders.load("shaders/irradiance.vert", "shaders/irradiance.frag"));
	auto cube = models.load("cube.fbx");
	auto cube_model = models.get(cube);

	int width = 2048;
	int height = 2048;

	unsigned int captureFBO;
	unsigned int captureRBO;
	glGenFramebuffers(1, &captureFBO);
	glGenRenderbuffers(1, &captureRBO);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

	unsigned int hdrTexture;

	// pbr: load the HDR environment map
	// ---------------------------------
	if (!skybox->capture_scene) {
		stbi_set_flip_vertically_on_load(true); //this seems different
		int width, height, nrComponents;
		float *data = stbi_loadf(assets.level.asset_path(skybox->filename).c_str(), &width, &height, &nrComponents, 0);
		if (data)
		{
			glGenTextures(1, &hdrTexture);
			glBindTexture(GL_TEXTURE_2D, hdrTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data); // note how we specify the texture's data value to be float

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			stbi_image_free(data);
		}
		else
		{
			log("Failed to load HDR image.");
		}
	}

	// pbr: setup cubemap to render to and attach to framebuffer
	// ---------------------------------------------------------
	unsigned int envCubemap;
	glGenTextures(1, &envCubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
	for (unsigned int i = 0; i < 6; ++i)
	{
		if (!skybox->capture_scene || take_capture)
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// pbr: set up projection and view matrices for capturing data onto the 6 cubemap face directions
	// ----------------------------------------------------------------------------------------------
	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	
	
	CubemapCapture capture;
	capture.width = width;
	capture.height = height;
	//capture.world = &world;
	capture.captureFBO = captureFBO;
	capture.captureTexture = envCubemap;
	//capture.capture_id = world.id_of(this);

	glm::mat4 captureViews[6];
	memcpy(captureViews, capture.captureViews, sizeof(glm::mat4) * 6);

	glm::quat captureViewsQ[] =
	{
		glm::quat(glm::radians(180.0f), glm::vec3(0,0.5,1.0)),
		glm::quat(glm::radians(180.0f), glm::vec3(0,-0.5,1.0)),
		glm::quat(glm::radians(180.0f), glm::vec3(0.5,1.0,0)),
		glm::quat(glm::radians(180.0f), glm::vec3(-0.5,1,0)),
		glm::quat(glm::radians(180.0f), glm::vec3(0,1,0)),
		glm::quat()
	};

	// pbr: convert HDR equirectangular environment map to cubemap equivalent
	// ----------------------------------------------------------------------
	if (!skybox->capture_scene) {
		equirectangular_to_cubemap_shader->bind();
		equirectangular_to_cubemap_shader->set_int("equirectangularMap", 0);
		equirectangular_to_cubemap_shader->set_mat4("projection", captureProjection);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, hdrTexture);
	}

	glViewport(0, 0, width, width); // don't forget to configure the viewport to the capture dimensions.
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

	GLfloat* pixels_float = NULL;
	uint8_t* pixels = NULL;
	
	if (take_capture) {
		pixels_float = new float[width * height * 3];
	}

	if (skybox->capture_scene) {
		pixels = new uint8_t[width * height * 3];
	}

	for (unsigned int i = 0; i < 6; ++i)
	{
		if (take_capture) {
			capture.capture(i);

			memset(pixels_float, 0, sizeof(float) * width * height * 3);
			memset(pixels, 0, sizeof(uint8_t) * width * height * 3);

			glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
			glReadPixels(0, 0, width, height, GL_RGB, GL_FLOAT, pixels_float);

			int index = 0;
			for (int j = 0; j < height; j++)
			{
				for (int i = 0; i < width; ++i)
				{
					float r = pixels_float[index];
					float g = pixels_float[index + 1];
					float b = pixels_float[index + 2];
					int ir = int(255.99 * r);
					int ig = int(255.99 * g);
					int ib = int(255.99 * b);

					pixels[index++] = ir;
					pixels[index++] = ig;
					pixels[index++] = ib;
				}
			}

			string_buffer save_capture_to = assets.level.asset_path(format("data/scene_capture/capture", i, ".jpg").c_str());
			//int success = stbi_write_jpg(save_capture_to.c_str(), width, height, 3, pixels, 100);
			
		}
		else if (skybox->capture_scene) {
			int width, height, num_channels;

			string_buffer load_capture_from = assets.level.asset_path(format("data/scene_capture/capture", i, ".jpg"));

			stbi_set_flip_vertically_on_load(false);
			uint8_t* data = stbi_load(load_capture_from.c_str(), &width, &height, &num_channels, 3);
			
			if (data == NULL) throw "Could not load scene capture!";

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glBindTexture(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap);
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
			glGenerateMipmap(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i);

			stbi_image_free(data);
		}
		else {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			equirectangular_to_cubemap_shader->set_mat4("view", captureViews[i]);
			
			RHI::bind_vertex_buffer(VERTEX_LAYOUT_DEFAULT);
			RHI::render_vertex_buffer(cube_model->meshes[0].buffer);
		}
	}

	delete pixels;
	delete pixels_float;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// pbr: create an irradiance cubemap, and re-scale capture FBO to irradiance scale.
	// --------------------------------------------------------------------------------
	unsigned int irradianceMap;
	glGenTextures(1, &irradianceMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
	for (unsigned int i = 0; i < 6; ++i)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);

	// pbr: solve diffuse integral by convolution to create an irradiance (cube)map.
	// -----------------------------------------------------------------------------
	irradiance_shader->bind();
	irradiance_shader->set_int("environmentMap", 0);
	irradiance_shader->set_mat4("projection", captureProjection);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

	glViewport(0, 0, 32, 32); // don't forget to configure the viewport to the capture dimensions.
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	for (unsigned int i = 0; i < 6; ++i)
	{
		irradiance_shader->set_mat4("view", captureViews[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		
		RHI::bind_vertex_buffer(VERTEX_LAYOUT_DEFAULT);
		RHI::render_vertex_buffer(cube_model->meshes[0].buffer);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	unsigned int prefilterMap;
	glGenTextures(1, &prefilterMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
	for (unsigned int i = 0; i < 6; ++i)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	auto prefilter_shader = shaders.get(shaders.load("shaders/prefilter.vert", "shaders/prefilter.frag"));

	prefilter_shader->bind();
	prefilter_shader->set_int("environmentMap", 0);
	prefilter_shader->set_mat4("projection", captureProjection);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	unsigned int maxMipLevels = 5;
	for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
	{
		// reisze framebuffer according to mip-level size.
		unsigned int mipWidth = 128 * std::pow(0.5, mip);
		unsigned int mipHeight = 128 * std::pow(0.5, mip);
		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
		glViewport(0, 0, mipWidth, mipHeight);

		float roughness = (float)mip / (float)(maxMipLevels - 1);
		prefilter_shader->set_float("roughness", roughness);
		for (unsigned int i = 0; i < 6; ++i)
		{
			prefilter_shader->set_mat4("view", captureViews[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap, mip);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			
			RHI::bind_vertex_buffer(VERTEX_LAYOUT_DEFAULT);
			RHI::render_vertex_buffer(cube_model->meshes[0].buffer);
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	auto brdf_shader = shaders.get(shaders.load("shaders/brdf_convultion.vert", "shaders/brdf_convultion.frag"));

	unsigned int brdfLUTTexture;
	glGenTextures(1, &brdfLUTTexture);

	// pre-allocate enough memory for the LUT texture.
	glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);

	glViewport(0, 0, 512, 512);
	brdf_shader->bind();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	render_quad();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	if (skybox->brdf_LUT.id == INVALID_HANDLE) {
		Texture brdf_LUT_as_texture;
		brdf_LUT_as_texture.filename = "brdf_LUT";
		brdf_LUT_as_texture.texture_id = brdfLUTTexture;

		Cubemap env_cubemap_as_texture;
		env_cubemap_as_texture.filename = "env_cubemap";
		env_cubemap_as_texture.texture_id = envCubemap;

		Cubemap irradiance_cubemap_as_texture;
		irradiance_cubemap_as_texture.filename = "iraddiance_map";
		irradiance_cubemap_as_texture.texture_id = irradianceMap;

		Cubemap prefilter_cubemap_as_texture;
		prefilter_cubemap_as_texture.filename = "prefilter_map";
		prefilter_cubemap_as_texture.texture_id = prefilterMap;

		skybox->brdf_LUT = textures.assign_handle(std::move(brdf_LUT_as_texture));
		//this->env_cubemap = make_Cubemap(std::move(env_cubemap_as_texture));
		skybox->irradiance_cubemap = cubemaps.assign_handle(std::move(irradiance_cubemap_as_texture));
		skybox->env_cubemap = cubemaps.assign_handle(std::move(env_cubemap_as_texture));
		skybox->prefilter_cubemap = cubemaps.assign_handle(std::move(prefilter_cubemap_as_texture));
	}
	else {
		textures.get(skybox->brdf_LUT)->texture_id = brdfLUTTexture;
		cubemaps.get(skybox->env_cubemap)->texture_id = envCubemap;
		cubemaps.get(skybox->irradiance_cubemap)->texture_id = irradianceMap;
		cubemaps.get(skybox->prefilter_cubemap)->texture_id = prefilterMap;
	}
#endif

	//todo update textures in place
}

//#include "lister.h"

Skybox* SkyboxSystem::make_default_Skybox(World& world, RenderCtx* params, string_view filename) {
	for (auto sky : world.filter<Skybox>(ANY_LAYER)) {
		if (sky->filename == filename) return sky;
	}

	auto id = world.make_ID();
	auto e = world.make<Entity>(id);
	e->layermask = GAME_LAYER;
	auto sky = world.make<Skybox>(id);

	//auto name = world.make<EntityEditor>(id);
	//name->name = "Skylight";

	sky->capture_scene = true;
	sky->filename = filename;

	auto skybox_shader = load_Shader(assets, "shaders/skybox.vert", "shaders/skybox.frag");
	auto cube_model = load_Model(assets, "cube.fbx");

	MaterialDesc mat{ skybox_shader };
	mat_vec3(mat, "skytop", glm::vec3(66, 188, 245) / 200.0f);
	mat_vec3(mat, "skyhorizon", glm::vec3(66, 135, 245) / 400.0f);

	auto materials = world.make<Materials>(id);
	
	materials->materials.append(make_Material(assets, mat));

	auto trans = world.make<Transform>(id);

	return sky;
}

SkyboxSystem::SkyboxSystem(Assets& assets, World& world) : assets(assets) {
	cube_model = load_Model(assets, "cube.fbx");

	world.on_make<Skybox>([this, &world](vector<ID>& created) {
		for (ID id : created) {
			load(world.by_id<Skybox>(id));
		}
	});
}

void SkyboxSystem::render(World& world, RenderCtx& ctx) {
	if (ctx.layermask & SHADOW_LAYER) return;

	for (ID id : world.filter<Skybox, Materials>(ctx.layermask)) {
		auto& materials = world.by_id<Materials>(id)->materials;
		
		ctx.command_buffer.draw(glm::mat4(1.0), cube_model, materials);
	}
}