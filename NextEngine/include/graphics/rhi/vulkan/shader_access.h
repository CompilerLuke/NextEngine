#pragma once

#pragma once

#include "volk.h"
#include "shader.h"

struct TextureDesc;

VkDescriptorPool make_DescriptorPool(VkDevice, VkPhysicalDevice);
void destroy_DescriptorPool(VkDescriptorPool);

struct UBOAllocator {

};



