#include "graphics/renderer/ibl.h"
#include "graphics/rhi/rhi.h"
#include "graphics/renderer/renderer.h"
#include "graphics/assets/assets.h"
#include "ecs/ecs.h"
#include <glm/gtc/matrix_transform.hpp>
#include "components/transform.h"
#include "core/io/logger.h"
#include "engine/vfs.h"
#include <stb_image.h>
#include "components/camera.h"
#include "components/lights.h"
#include "graphics/assets/material.h"
#include "graphics/renderer/lighting_system.h"

#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/frame_buffer.h"
#include "graphics/rhi/vulkan/draw.h"

#include "graphics/assets/assets_store.h"

struct CubemapViewPushConstant {
	glm::mat4 projection;
	glm::mat4 view;
};

struct CubemapPassResources {
    VkRenderPass wait_after_render_pass;
    VkRenderPass no_wait_render_pass;
    VkImage depth_image;
    VkImageView depth_image_view;
    sampler_handle sampler;
    VkFormat depth_format;
    VkFormat color_format;
};

VkRenderPass make_color_and_depth_render_pass(VkDevice device, VkAttachmentDescription attachments[2], bool wait_after) {
	VkAttachmentReference color_attachment = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference depth_attachment = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	VkSubpassDependency next_dependency = {};
	next_dependency.srcSubpass = 0;
	next_dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
	next_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	next_dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	next_dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	next_dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	next_dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	//todo copied from vk_framegraph, extract into function
	VkSubpassDependency write_depth_dependency = {};
	write_depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	write_depth_dependency.dstSubpass = 0;
	write_depth_dependency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	write_depth_dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	write_depth_dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	write_depth_dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	write_depth_dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkSubpassDependency write_color_dependency = {};
	write_color_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	write_color_dependency.dstSubpass = 0;
	write_color_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	write_color_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	write_color_dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	write_color_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	write_color_dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkSubpassDependency dependencies[3] = { write_depth_dependency, write_color_dependency, next_dependency };

	VkSubpassDescription subpass = {};
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment;
	subpass.pDepthStencilAttachment = &depth_attachment;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = 2;
	render_pass_info.pAttachments = attachments;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	render_pass_info.dependencyCount = wait_after ? 3 : 2;
	render_pass_info.pDependencies = dependencies;

	VkRenderPass render_pass;
	VK_CHECK(vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass));
	return render_pass;
}


void begin_render_pass(VkCommandBuffer cmd_buffer, VkRenderPass render_pass, VkFramebuffer framebuffer, uint width, uint height, slice<VkClearValue> clear_values) {
	printf("RENDERING TO RENDER PASS %p\n", render_pass);
	
	VkRenderPassBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	begin_info.clearValueCount = clear_values.length;
	begin_info.pClearValues = clear_values.data;
	begin_info.renderPass = render_pass;
	begin_info.framebuffer = framebuffer;
	begin_info.renderArea = { 0,0,width,height };

	//todo disable dynamic viewport
	VkViewport viewport = { 0,0,(float)width,(float)height,0,1 };
	vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

	VkRect2D scissor{ 0,0,width,height };
	vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

	vkCmdBeginRenderPass(cmd_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}


CubemapPassResources* make_cubemap_pass_resources() {
    CubemapPassResources& resources = *PERMANENT_ALLOC(CubemapPassResources);
    
	resources.color_format = VK_FORMAT_R32G32B32A32_SFLOAT;
	resources.depth_format = find_depth_format(rhi.device, false);

	SamplerDesc sampler_desc = {};
	sampler_desc.wrap_u = Wrap::ClampToBorder;
	sampler_desc.wrap_v = Wrap::ClampToBorder;
	sampler_desc.min_filter = Filter::Linear;
	sampler_desc.mag_filter = Filter::Linear;
	sampler_desc.mip_mode = Filter::Linear;

	VkAttachmentDescription attachments[2] = { default_color_attachment, default_depth_attachment };
	attachments[0].format = resources.color_format;
	attachments[1].format = resources.depth_format;

	resources.wait_after_render_pass = make_color_and_depth_render_pass(rhi.device, attachments, true);
	resources.no_wait_render_pass = make_color_and_depth_render_pass(rhi.device, attachments, false);
	resources.sampler = query_Sampler(sampler_desc);

	printf("CREATED RENDER PASS %p\n", resources.wait_after_render_pass);
	printf("CREATED RENDER PASS %p\n", resources.no_wait_render_pass);
                                                       
    return &resources;
}

const uint MAX_RENDER_TO_CUBEMAP_MIP = 5;

struct CubemapRenderTarget {
	uint width, height;
	uint mips = 1;
	VkFramebuffer framebuffers[6];
	VkImageView face_views[6];
	VkImage depth_image;
	VkImageView depth_view;
	VkImage image;
	VkImageView view;
};

void make_cubemap_images(CubemapRenderTarget& cubemap, VkFormat color_format, VkFormat depth_format) {
	VkDevice device = rhi.device;
	VkPhysicalDevice physical_device = rhi.device;

	//CREATE DEPTH IMAGE
	VkImageCreateInfo depth_info = image_create_default;
	depth_info.extent = { cubemap.width, cubemap.height, 1 };
	depth_info.format = depth_format;
	depth_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VK_CHECK(vkCreateImage(device, &depth_info, nullptr, &cubemap.depth_image));
	alloc_and_bind_memory(rhi.texture_allocator, cubemap.depth_image);

	//CREATE CUBEMAP IMAGE
	VkImageCreateInfo imageInfo = image_create_default;
	imageInfo.extent = { cubemap.width, cubemap.height, 1 };
	imageInfo.mipLevels = cubemap.mips;
	imageInfo.arrayLayers = 6;
	imageInfo.format = color_format;
	imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &cubemap.image));
	alloc_and_bind_memory(rhi.texture_allocator, cubemap.image);

	//CREATE IMAGE VIEWS
	cubemap.depth_view = make_ImageView(device, cubemap.depth_image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);

	VkImageViewCreateInfo image_view_info = image_view_create_default;
	image_view_info.format = color_format;
	image_view_info.image = cubemap.image;
	image_view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	image_view_info.subresourceRange.layerCount = 6;
	image_view_info.subresourceRange.levelCount = cubemap.mips;
	image_view_info.subresourceRange.baseMipLevel = 0;

	VK_CHECK(vkCreateImageView(device, &image_view_info, nullptr, &cubemap.view));
}

void make_cubemap_framebuffer(CubemapRenderTarget& cubemap, VkFormat color_format, VkRenderPass render_pass, uint render_to_mip) {
	VkDevice device = rhi.device;
	VkPhysicalDevice physical_device = rhi.device;
	
	for (uint i = 0; i < 6; i++) {
		VkImageViewCreateInfo image_view_info = image_view_create_default;
		image_view_info.format = color_format;
		image_view_info.image = cubemap.image;
		image_view_info.subresourceRange.baseArrayLayer = i;
		image_view_info.subresourceRange.baseMipLevel = render_to_mip;

		VK_CHECK(vkCreateImageView(device, &image_view_info, nullptr, &cubemap.face_views[i]));

		VkImageView views[2] = { cubemap.face_views[i], cubemap.depth_view };

		VkFramebufferCreateInfo framebuffer_info = {};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.attachmentCount = 2;
		framebuffer_info.pAttachments = views;
		framebuffer_info.width = cubemap.width;
		framebuffer_info.height = cubemap.height;
		framebuffer_info.layers = 1;
		framebuffer_info.renderPass = render_pass;

		VK_CHECK(vkCreateFramebuffer(device, &framebuffer_info, nullptr, &cubemap.framebuffers[i]));
	}
}

void make_cubemap_render_target(CubemapRenderTarget& cubemap, VkFormat color_format, VkFormat depth_format, VkRenderPass render_pass) {
	make_cubemap_images(cubemap, color_format, depth_format);
	make_cubemap_framebuffer(cubemap, color_format, render_pass, 0);
}

//pipeline_handle
pipeline_handle make_no_cull_pipeline(VkRenderPass render_pass, shader_handle shader, array<2, PushConstantRange> ranges) {
	//CREATE PIPELINE AND DESCRIPTORS
    
    GraphicsPipelineDesc pipeline_desc = {};
	pipeline_desc.shader = shader;
	pipeline_desc.state = Cull_None;
	pipeline_desc.render_pass = { (u64)render_pass };
	memcpy_t(pipeline_desc.range, ranges.data, 2);

    return query_Pipeline(pipeline_desc);
}

Cubemap cubemap_from_target(CubemapRenderTarget target) {
	Cubemap result = {};
	result.image = target.image;
	result.view = target.view;

	return result;
}

void render_to_cubemap_mip(VkRenderPass render_pass, CubemapRenderTarget& cubemap, CommandBuffer& cmd_buffer, bool flip = false) {
	glm::mat4 capture_views[] =
	{
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))
	};

	glm::mat4 capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	if (flip) {
		capture_projection[1][1] *= -1;
		std::swap(capture_views[2], capture_views[3]);
	}

	for (uint i = 0; i < 6; i++) {
		glm::mat4 proj_view = capture_projection * capture_views[i];

		push_constant(cmd_buffer, VERTEX_STAGE, 0, &proj_view);

		VkClearValue clear_values[2] = {};
		clear_values[0].color = { 0,0,0,1 };
		clear_values[1].depthStencil = { 1,0 };

		begin_render_pass(cmd_buffer, render_pass, cubemap.framebuffers[i], cubemap.width, cubemap.height, { clear_values, 2 });
		draw_mesh(cmd_buffer, primitives.cube_buffer);
		vkCmdEndRenderPass(cmd_buffer);
	}
}

void render_to_cubemap(VkRenderPass render_pass, CubemapRenderTarget& cubemap, shader_handle shader, descriptor_set_handle descriptor, bool flip = false) {

    pipeline_handle pipeline = make_no_cull_pipeline(render_pass, shader, { PushConstantRange{ 0, sizeof(glm::mat4) }});
	
	CommandBuffer cmd_buffer;
	cmd_buffer.cmd_buffer = begin_recording(rhi.background_graphics);
    
    bind_pipeline(cmd_buffer, pipeline);
	bind_descriptor(cmd_buffer, 0, descriptor);
	bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);

	render_to_cubemap_mip(render_pass, cubemap, cmd_buffer, flip);

	end_recording(rhi.background_graphics, cmd_buffer);
}

cubemap_handle convert_equirectangular_to_cubemap(CubemapPassResources& resources, texture_handle env_map) {
	VkPhysicalDevice physical_device = rhi.device;
	VkDevice device = rhi.device;

	CubemapRenderTarget render_target = {512, 512};
	make_cubemap_render_target(render_target, resources.color_format, resources.depth_format, resources.wait_after_render_pass);

	DescriptorDesc equirectangular_to_cubemap_desc{};
	add_combined_sampler(equirectangular_to_cubemap_desc, FRAGMENT_STAGE, resources.sampler, env_map, 1);

	descriptor_set_handle descriptor;
	update_descriptor_set(descriptor, equirectangular_to_cubemap_desc);

	render_to_cubemap(resources.wait_after_render_pass, render_target, load_Shader("shaders/eToCubemap.vert", "shaders/eToCubemap.frag"), descriptor, true);

	return assets.cubemaps.assign_handle(cubemap_from_target(render_target));
}

cubemap_handle compute_irradiance(CubemapPassResources& resources, cubemap_handle env_map) {
	CubemapRenderTarget render_target = { 64, 64 };
	make_cubemap_render_target(render_target, resources.color_format, resources.depth_format, resources.no_wait_render_pass);

	DescriptorDesc descriptor_desc{};
	add_combined_sampler(descriptor_desc, FRAGMENT_STAGE, resources.sampler, env_map, 0);
	
	descriptor_set_handle descriptor;
	update_descriptor_set(descriptor, descriptor_desc);

	render_to_cubemap(resources.no_wait_render_pass, render_target, load_Shader("shaders/irradiance.vert", "shaders/irradiance.frag"), descriptor);
    
    return assets.cubemaps.assign_handle(cubemap_from_target(render_target));
}

cubemap_handle compute_reflection(CubemapPassResources& resources, cubemap_handle env_map) {
    uint width = 256;
    uint max_mip_levels = 7;
	CubemapRenderTarget cubemap_images = {width,width,max_mip_levels};
	make_cubemap_images(cubemap_images, resources.color_format, resources.depth_format);

	DescriptorDesc descriptor_desc{};
	add_combined_sampler(descriptor_desc, FRAGMENT_STAGE, resources.sampler, env_map, 0);

	descriptor_set_handle descriptor;
	update_descriptor_set(descriptor, descriptor_desc);

	
	GraphicsPipelineDesc pipeline_desc = {};
	pipeline_desc.shader = load_Shader("shaders/prefilter.vert", "shaders/prefilter.frag");
	pipeline_desc.render_pass = { (u64)resources.wait_after_render_pass };
	pipeline_desc.state = Cull_None;
	pipeline_desc.range[0] = {0, sizeof(glm::mat4)};
	pipeline_desc.range[1] = { sizeof(glm::mat4), sizeof(float) };

	pipeline_handle pipeline = query_Pipeline(pipeline_desc);

	CommandBuffer cmd_buffer = { begin_recording(rhi.background_graphics) };
	bind_pipeline(cmd_buffer, pipeline);
	bind_descriptor(cmd_buffer, 0, descriptor);
	bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);

	const uint roughness_offset = sizeof(glm::mat4);

	for (uint mip = 0; mip < max_mip_levels; mip++) {
		CubemapRenderTarget target = cubemap_images;
		target.width = cubemap_images.width * std::pow(0.5, mip);
		target.height = cubemap_images.height * std::pow(0.5, mip);

		float roughness = (float)mip / (float)(max_mip_levels - 1);
		
		push_constant(cmd_buffer, FRAGMENT_STAGE, roughness_offset, &roughness);		
		make_cubemap_framebuffer(target, resources.color_format, resources.wait_after_render_pass, mip);
		render_to_cubemap_mip(resources.wait_after_render_pass, target, cmd_buffer);
	}

	end_recording(rhi.background_graphics, cmd_buffer);

	return assets.cubemaps.assign_handle(cubemap_from_target(cubemap_images));
}

texture_handle compute_brdf_lut(uint resolution) {
	
	//return load_Texture("brdf.png");

	texture_handle brdf;

	FramebufferDesc framebuffer_desc = {resolution, resolution};
	framebuffer_desc.depth_buffer = DepthBufferFormat::None;
	add_color_attachment(framebuffer_desc, &brdf);

	SubpassDesc subpass = {};
	subpass.color_attachments.append(0);
	subpass.depth_attachment = false;

	RenderPassInfo info = {};
	VkRenderPass render_pass = make_RenderPass(rhi.device, rhi.device, framebuffer_desc, subpass);
	VkFramebuffer framebuffer = make_Framebuffer(rhi.device, rhi.device, render_pass, framebuffer_desc, info);

	pipeline_handle pipeline = make_no_cull_pipeline(render_pass, load_Shader("shaders/brdf_convultion.vert", "shaders/brdf_convultion.frag"), {});

	VkClearValue clear_value = {};
	clear_value.color = { 0.0f, 0.0f, 0.0f, 1.0f };

	CommandBuffer cmd_buffer{ begin_recording(rhi.background_graphics) };
	bind_pipeline(cmd_buffer, pipeline);
	bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);

	begin_render_pass(cmd_buffer, render_pass, framebuffer, resolution, resolution, clear_value);
	draw_mesh(cmd_buffer, primitives.quad_buffer);
	vkCmdEndRenderPass(cmd_buffer);

	end_recording(rhi.background_graphics, cmd_buffer);

	return brdf;
}



void extract_lighting_from_cubemap(LightingSystem& lighting_system, SkyLight& skylight) {
    skylight.irradiance = compute_irradiance(*assets.cubemap_pass_resources, skylight.cubemap);
	skylight.prefilter = compute_reflection(*assets.cubemap_pass_resources, skylight.cubemap);
}

void recompute_lighting_from_cubemap(LightingSystem& lighting_system, SkyLight& skylight) {
	cubemap_handle irradiance = compute_irradiance(*assets.cubemap_pass_resources, skylight.cubemap);
	cubemap_handle prefilter = compute_reflection(*assets.cubemap_pass_resources, skylight.cubemap);

    //TODO NO FUNCTION SUPPORT YET
    return;
    
	//*assets.cubemaps.get(skylight.cubemap) = irradiance;
	//*assets.cubemaps.get(skylight.prefilter) = prefilter;

	DescriptorDesc desc{};
	add_combined_sampler(desc, FRAGMENT_STAGE, assets.cubemap_pass_resources->sampler, skylight.irradiance, 1);
	add_combined_sampler(desc, FRAGMENT_STAGE, assets.cubemap_pass_resources->sampler, skylight.prefilter, 2);

	update_descriptor_set(lighting_system.pbr_descriptor[get_frame_index()], desc);
}

ID make_default_Skybox(World& world, string_view filename) {
	cubemap_handle env_map = load_HDR(filename);

	auto[e, trans, sky, skylight, materials] = world.make<Transform, Skybox, SkyLight, Materials>();

	sky.cubemap = env_map;
	skylight.capture_scene = false;
	skylight.cubemap = env_map;

	skylight.irradiance = compute_irradiance(*assets.cubemap_pass_resources, env_map);
	skylight.prefilter = compute_reflection(*assets.cubemap_pass_resources, env_map);


	//auto name = world.make<EntityEditor>(id);
	//name->name = "Skylight";

	skylight.capture_scene = true;

	auto skybox_shader = load_Shader("shaders/skybox.vert", "shaders/skybox.frag");


	MaterialDesc mat{ skybox_shader };
	mat.draw_state = Cull_None | DepthFunc_Lequal;
	
	//mat_cubemap(mat, "environmentMap", sky.cubemap);
    
    mat.mat_vec3("skyhorizon", glm::vec3(66, 188, 245) / 180.0f);
    mat.mat_vec3("skytop", glm::vec3(66, 188, 245) / 200.0f);

	//mat_vec3(mat, "skyhorizon", glm::vec3(66, 188, 245) / 200.0f);
	//mat_vec3(mat, "skytop", glm::vec3(66, 135, 245) / 300.0f);

	materials.materials.append(make_Material(mat));

	return e.id;
}

void extract_skybox(SkyboxRenderData& data, World& world, EntityQuery layermask) {
	data.material = { INVALID_HANDLE };
	
	for (auto [e, trans, skybox, materials] : world.filter<Transform, Skybox, Materials>(layermask)) {
		data.position = trans.position;
		data.material = materials.materials[0];

		break;
	}
}

void render_skybox(const SkyboxRenderData& data, RenderPass& ctx) {	
	if (data.material.id == INVALID_HANDLE) return;
	return;

	Transform trans{ data.position };
	material_handle material_handle = data.material;
	draw_mesh(*ctx.cmd_buffer, primitives.cube, material_handle, trans);
}




//======================== DELETE CODE =====================================

/*
void load_Skybox(string_view view) { //todo cleanup
	if (skybox->cubemap != INVALID_HANDLE) return;

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
}*/

//#include "lister.h"

//void SkyboxSystem::bind_ibl_params(ShaderConfig& config, RenderPass& ctx) {
	//Skybox* skybox = ctx.skybox;
	//auto bind_to = ctx.command_buffer.next_texture_index();

	/*gl_bind_cubemap(assets.cubemaps, skybox->irradiance_cubemap, bind_to);
	config.set_int("irradianceMap", bind_to);

	bind_to = ctx.command_buffer.next_texture_index();
	gl_bind_cubemap(assets.cubemaps, skybox->prefilter_cubemap, bind_to);
	config.set_int("prefilterMap", bind_to);

	bind_to = ctx.command_buffer.next_texture_index();
	gl_bind_to(assets.textures, skybox->brdf_LUT, bind_to);
	config.set_int("brdfLUT", bind_to);*/
	//}

struct CubemapCapture {
	RenderPass* params;
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

		ID main_camera = get_camera(world, EntityQuery());

		RenderPass new_params = *params;
		new_params.viewport.width = width;
		new_params.viewport.height = height;

		auto[e, new_trans, camera] = world.make<Transform, Camera>();
		new_trans.position = world.by_id<Transform>(capture_id)->position;
		new_trans.rotation = glm::inverse(glm::quat_cast(captureViews[i])); //captureViewsQ[i];

		camera.far_plane = 600;
		camera.near_plane = 0.1f;
		camera.fov = 90.0f;


		/* world.by_id<Entity>(main_camera)->enabled = false; */

		update_camera_matrices(new_trans, camera, new_params.viewport);

		//((MainPass*)new_params.pass)->render_to_buffer(world, new_params, [this, i]() {
			//glViewport(0, 0, width, width); // don't forget to configure the viewport to the capture dimensions.
			//glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
			//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, captureTexture, 0);
			//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		//});

		world.free_now_by_id(e.id);

		/* world.by_id<Entity>(main_camera)->enabled = true; */
	}
};
