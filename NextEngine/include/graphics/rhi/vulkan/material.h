#pragma once

#include "graphics/rhi/vulkan/volk.h"
#include "graphics/assets/material.h"
#include "graphics/rhi/vulkan/shader_access.h"

struct RHI;
struct Assets;

struct Material {
	MaterialPipelineInfo info;
	UBOBuffer ubos[MAX_FRAMES_IN_FLIGHT] = {};
	descriptor_set_handle sets[MAX_FRAMES_IN_FLIGHT] = {};
	uint index = 0;
	bool requires_depth_descriptor = true;
};

struct MaterialAllocator {
	VkDevice device;
	VkPhysicalDevice physical_device;
	DescriptorPool& descriptor_pool;
	sampler_handle default_sampler;

	MaterialAllocator(VkDevice, VkPhysicalDevice, DescriptorPool&);
	void init();
	~MaterialAllocator();

	void make(MaterialDesc& desc, Material* material);
	void update(MaterialDesc& from, MaterialDesc& to, Material* material);
};

//void make_MaterialAllocator(MaterialAllocator&);
//void destroy_MaterialAllocator(MaterialAllocator&);
//Material make_Material(MaterialAllocator&, MaterialDesc&);

Material* get_Material(material_handle handle);

const int MATERIAL_SET = 2;