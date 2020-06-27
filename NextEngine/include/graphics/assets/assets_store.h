#pragma once

#include "core/container.h"
#include "core/container/handle_manager.h"
#include "core/container/queue.h"

#include "graphics/assets/shader.h"
#include "graphics/assets/model.h"
#include "graphics/assets/texture.h"
#include "graphics/renderer/lighting_system.h"

#ifdef RENDER_API_VULKAN
#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/shader.h"
#include "graphics/rhi/vulkan/material.h"
#include "graphics/rhi/vulkan/texture.h"
#include "graphics/rhi/vulkan/draw.h"
#include "graphics/renderer/ibl.h"
#endif

struct LoadTextureJob {
	
};

struct EquirectangularToCubemapJob {
	cubemap_handle handle;
	texture_handle env_map;
};

struct Assets {
	//struct ShaderCompiler* shader_compiler;
	//struct VertexStreaming* buffer_allocator;
	//struct TextureAllocator* texture_allocator;
	//struct MaterialAllocator* material_allocator;

	string_buffer asset_path;
	hash_map<sstring, uint, 1000> path_to_handle;

	HandleManager<Model, model_handle> models;
	HandleManager<Shader, shader_handle> shaders;
	HandleManager<Texture, texture_handle> textures;
	HandleManager<Cubemap, cubemap_handle> cubemaps;
	HandleManager<Material, material_handle> materials;

	queue<LoadTextureJob, 20> load_texture_jobs;
	queue<EquirectangularToCubemapJob, 5> equirectangular_to_cubemap_jobs;

	CubemapPassResources cubemap_pass_resources;
};

extern Assets assets;