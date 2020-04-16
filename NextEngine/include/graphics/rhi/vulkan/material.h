#pragma once

#include "graphics/rhi/vulkan/volk.h"
#include "graphics/assets/material.h"
#include "graphics/rhi/vulkan/shader.h"

struct RHI;
struct Assets;

struct Material {
	MaterialDesc desc;
	VkDeviceMemory memory;
	VkBuffer ubo;
	VkDescriptorSetLayout layout;
	VkDescriptorSet set;

	array<MAX_SAMPLERS, VkSampler> samplers;
};

Material* get_Material(Assets& assets, material_handle handle);
Material make_material_descriptors(RHI& rhi, Assets& assets, MaterialDesc& desc);