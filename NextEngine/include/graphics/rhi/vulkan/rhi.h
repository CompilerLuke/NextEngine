#pragma once

#include "volk.h"

struct RHI;
struct BufferAllocator;

VkDevice get_Device(RHI&);
VkPhysicalDevice get_PhysicalDevice(RHI&);
VkInstance get_Instance(RHI&);
BufferAllocator& get_BufferAllocator(RHI&);
VkCommandPool get_CommandPool(RHI&);
VkQueue get_GraphicsQueue(RHI&);
