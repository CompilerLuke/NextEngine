#pragma once

#include "graphics/rhi/vulkan/volk.h"
#include "graphics/assets/material.h"

struct Material {
	MaterialDesc desc;
	VkDeviceMemory memory;
	VkBuffer ubo;
	VkDescriptorSetLayout layout;
	VkDescriptorSet set;
};