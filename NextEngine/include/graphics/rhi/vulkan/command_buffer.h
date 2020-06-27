#pragma once

#include "core.h"
#include "core/container/array.h"

struct QueueSubmitInfo;
struct Device;

#define MAX_COMMAND_BUFFERS 30

//Command Pool must be externally sychronized
//including waiting for frames of command buffers to have completed
//as if frame index overlap with still executing frames, there will be a memory corruption
struct CommandPool {
	VkPhysicalDevice physical_device;
	VkDevice device;
	VkQueue queue;
	VkCommandPool pool;
	array<MAX_COMMAND_BUFFERS, VkCommandBuffer> free;
	array<MAX_COMMAND_BUFFERS, VkCommandBuffer> submited[MAX_FRAMES_IN_FLIGHT];
	uint frame_index;
};

void make_CommandPool(CommandPool&, Device& device, QueueType queue_type, uint count);
void destroy_CommandPool(CommandPool&);

VkCommandBuffer begin_recording(CommandPool&);
void end_recording(CommandPool&, VkCommandBuffer);

void completed_frame(CommandPool&, uint frame_index);

void begin_frame(CommandPool&, uint frame_index);
void end_frame(CommandPool&);

void submit_all_cmds(CommandPool&, QueueSubmitInfo&);

void make_vk_CommandPool(VkCommandPool&, Device& device, QueueType queue_type);
void alloc_CommandBuffers(VkDevice device, VkCommandPool, uint count, VkCommandBuffer* result);
