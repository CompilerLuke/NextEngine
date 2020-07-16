#pragma once

#include "volk.h"
#include <stdlib.h>

enum QueueType { Queue_Graphics, Queue_AsyncCompute, Queue_AsyncTransfer, Queue_Present, Queue_Count };

#define VK_CHECK(x)  \
	if (x != VK_SUCCESS) { \
		fprintf(stderr, "Vulkan Error at %s:%d", __FILE__, __LINE__); \
		abort(); \
	}
