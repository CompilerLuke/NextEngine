#pragma once

#include "volk.h"

//number of frames already submitted to the GPU

#define VK_CHECK(x)  \
{ \
	VkResult result = x; \
	if (result != VK_SUCCESS) { \
		fprintf(stderr, "Vulkan Error %d at %s:%d", result, __FILE__, __LINE__); \
		abort(); \
	} \
}
