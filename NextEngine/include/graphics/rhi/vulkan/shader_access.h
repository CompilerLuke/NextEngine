#pragma once

#pragma once

#include "volk.h"
#include "shader.h"


struct DescriptorCount {
	uint max_sets = 20;
	uint max_ubos = 10;
	uint max_samplers = 10;
};

struct DescriptorPool {
	VkDevice device;
	VkPhysicalDevice physical_device;
	VkDescriptorPool pool;

	operator VkDescriptorPool() { return pool;  }
};

#define MAX_UBO_POW_2_SIZE 10

/*
struct UBOAllocInfo {
	UBOAllocInfo* next;
};

struct UBOAllocator {
	UBOAllocInfo* aligned_free_list[MAX_UBO_POW_2_SIZE];
};
*/

struct DescriptorBinding {
	struct ImageInfo {
		uint binding;
		slice<VkDescriptorImageInfo> info;
	};

	struct BufferInfo {
		uint binding;
		slice<VkDescriptorBufferInfo> info;
	};
	
	slice<BufferInfo> ubos;
	slice<ImageInfo> samplers;
};

VkShaderStageFlags to_vk_stage(Stage stage);

VkDescriptorPool vk_make_DescriptorPool(VkDevice, VkPhysicalDevice);
void vk_destroy_DescriptorPool(VkDescriptorPool);

void make_DescriptorPool(DescriptorPool& pool, VkDevice, VkPhysicalDevice, const DescriptorCount&);
void destroy_DescriptorPool(DescriptorPool&);

VkDescriptorSet get_descriptor_set(descriptor_set_handle);
VkDescriptorSetLayout get_descriptor_set_layout(descriptor_set_handle);

//VkDescriptorSet make_DescriptorSet(DescriptorPool& pool, slice<VkDescriptorSetLayout> set_layouts);
//VkDescriptorSetLayout make_DescriptorSetLayout(VkDevice device, VkDescriptorPool pool, DescriptorSetInfo& info);
//void update_DescriptorSet(VkDevice device, VkDescriptorSet descriptor_set, DescriptorSetInfo& info, DescriptorBinding& binding);


VkDescriptorSet make_set(DescriptorPool&, slice<VkDescriptorSetLayout> set_layouts);

VkDescriptorSetLayout make_set_layout(VkDevice, slice<VkDescriptorSetLayoutBinding> bindings);
VkDescriptorSetLayout make_set_layout(VkDevice device, DescriptorSetInfo& info);

void destroy_SetLayout(VkDevice, VkDescriptorSetLayout);

//void make_UBOAllocator(UBOAllocator&);
//void destroy_UBOAllocator(UBOAllocator&);


