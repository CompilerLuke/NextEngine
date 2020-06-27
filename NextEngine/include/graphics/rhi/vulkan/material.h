#pragma once

#include "graphics/rhi/vulkan/volk.h"
#include "graphics/assets/material.h"
#include "graphics/rhi/vulkan/shader_access.h"

struct RHI;
struct Assets;

struct Material {
	MaterialDesc desc;
	UBOBuffer ubo;
	descriptor_set_handle set;
};

struct MaterialAllocator {
	VkDevice device;
	VkPhysicalDevice physical_device;
	DescriptorPool& descriptor_pool;
	sampler_handle default_sampler;

	MaterialAllocator(VkDevice, VkPhysicalDevice, DescriptorPool&);
	void init();
	~MaterialAllocator();

	Material make(MaterialDesc& desc);
};

//void make_MaterialAllocator(MaterialAllocator&);
//void destroy_MaterialAllocator(MaterialAllocator&);
//Material make_Material(MaterialAllocator&, MaterialDesc&);

Material* get_Material(material_handle handle);

const int MATERIAL_SET = 2;