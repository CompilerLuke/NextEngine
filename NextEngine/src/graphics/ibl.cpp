#include "stdafx.h"
#include "graphics/ibl.h"
#include "graphics/draw.h"
#include "graphics/materialSystem.h"
#include "ecs/ecs.h"
#include "model/model.h"
#include "graphics/texture.h"
#include "graphics/frameBuffer.h"
#include "glad/glad.h"
#include <glm/gtc/matrix_transform.hpp>
#include "model/model.h"
#include "graphics/primitives.h"
#include "components/transform.h"
#include "graphics/rhi.h"
#include "logger/logger.h"
#include "core/vfs.h"
#include <stb_image.h>

REFLECT_STRUCT_BEGIN(Skybox)
REFLECT_STRUCT_MEMBER(filename)
REFLECT_STRUCT_MEMBER(env_cubemap)
REFLECT_STRUCT_MEMBER(irradiance_cubemap)
REFLECT_STRUCT_MEMBER(prefilter_cubemap)
REFLECT_STRUCT_MEMBER(brdf_LUT)
REFLECT_STRUCT_END()

struct DrawCommandState skybox_draw_state = {
	Cull_None,
	DepthFunc_Lequal,
	false,
	draw_skybox
};

void Skybox::set_ibl_params(Handle<Shader> shader, World& world, RenderParams& params) {
	auto bind_to = params.command_buffer->next_texture_index();
	cubemap::bind_to(irradiance_cubemap, bind_to);
	shader::set_int(shader, "irradianceMap", bind_to);
	
	bind_to = params.command_buffer->next_texture_index();
	cubemap::bind_to(prefilter_cubemap, bind_to);
	shader::set_int(shader, "prefilterMap", bind_to);

	bind_to = params.command_buffer->next_texture_index();
	texture::bind_to(brdf_LUT, bind_to);
	shader::set_int(shader, "brdfLUT", bind_to);
}

void Skybox::on_load(World& world) {
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	auto equirectangular_to_cubemap_shader = load_Shader("shaders/eToCubemap.vert", "shaders/eToCubemap.frag");
	auto irradiance_shader = load_Shader("shaders/irradiance.vert", "shaders/irradiance.frag");
	auto cube = load_Model("cube.fbx");
	auto cube_model = RHI::model_manager.get(cube);

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
	{
		stbi_set_flip_vertically_on_load(true);
		int width, height, nrComponents;
		float *data = stbi_loadf(Level::asset_path(filename).c_str(), &width, &height, &nrComponents, 0);
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
	glm::mat4 captureViews[] =
	{
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};

	// pbr: convert HDR equirectangular environment map to cubemap equivalent
	// ----------------------------------------------------------------------
	shader::bind(equirectangular_to_cubemap_shader);
	shader::set_int(equirectangular_to_cubemap_shader, "equirectangularMap", 0);
	shader::set_mat4(equirectangular_to_cubemap_shader, "projection", captureProjection);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdrTexture);

	glViewport(0, 0, width, width); // don't forget to configure the viewport to the capture dimensions.
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	for (unsigned int i = 0; i < 6; ++i)
	{
		shader::set_mat4(equirectangular_to_cubemap_shader, "view", captureViews[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		cube_model->meshes[0].buffer.bind();

		glDrawElements(GL_TRIANGLES, cube_model->meshes[0].buffer.length, GL_UNSIGNED_INT, 0);
	}
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
	shader::bind(irradiance_shader);
	shader::set_int(irradiance_shader, "environmentMap", 0);
	shader::set_mat4(irradiance_shader, "projection", captureProjection);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

	glViewport(0, 0, 32, 32); // don't forget to configure the viewport to the capture dimensions.
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	for (unsigned int i = 0; i < 6; ++i)
	{
		shader::set_mat4(irradiance_shader, "view", captureViews[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		cube_model->meshes[0].buffer.bind();
		glDrawElements(GL_TRIANGLES, cube_model->meshes[0].buffer.length, GL_UNSIGNED_INT, 0);
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

	auto prefilter_shader = load_Shader("shaders/prefilter.vert", "shaders/prefilter.frag");

	shader::bind(prefilter_shader);
	shader::set_int(prefilter_shader, "environmentMap", 0);
	shader::set_mat4(prefilter_shader, "projection", captureProjection);

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
		shader::set_float(prefilter_shader, "roughness", roughness);
		for (unsigned int i = 0; i < 6; ++i)
		{
			shader::set_mat4(prefilter_shader, "view", captureViews[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap, mip);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			cube_model->meshes[0].buffer.bind();
			glDrawElements(GL_TRIANGLES, cube_model->meshes[0].buffer.length, GL_UNSIGNED_INT, 0);
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	auto brdf_shader = load_Shader("shaders/brdf_convultion.vert", "shaders/brdf_convultion.frag");

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
	shader::bind(brdf_shader);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	render_quad();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

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

	this->brdf_LUT = make_Texture(std::move(brdf_LUT_as_texture));
	this->env_cubemap = make_Cubemap(std::move(env_cubemap_as_texture));
	this->irradiance_cubemap = make_Cubemap(std::move(irradiance_cubemap_as_texture));
	this->env_cubemap = make_Cubemap(std::move(env_cubemap_as_texture));
	this->prefilter_cubemap = make_Cubemap(std::move(prefilter_cubemap_as_texture));

	return;
}

Skybox* load_Skybox(World& world, StringView filename) {
	for (auto sky : world.filter<Skybox>(any_layer)) {
		if (sky->filename == filename) return sky;
	}

	auto id = world.make_ID();
	auto e = world.make<Entity>(id);
	e->layermask = game_layer;
	auto sky = world.make<Skybox>(id);

	sky->filename = filename;
	sky->on_load(world);

	auto skybox_shader = load_Shader("shaders/skybox.vert", "shaders/skybox.frag");
	auto cube_model = load_Model("cube.fbx");

	Material mat;
	mat.shader = skybox_shader;
	mat.params.append(make_Param_Cubemap(location(skybox_shader, "environmentMap"), sky->env_cubemap));
	mat.state = &skybox_draw_state;
	
	auto model_renderer = world.make<ModelRenderer>(id);
	model_renderer->model_id = cube_model;

	auto materials = world.make<Materials>(id);
	
	materials->materials.append(RHI::material_manager.make(std::move(mat)));

	auto trans = world.make<Transform>(id);

	return sky;
}