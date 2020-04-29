#pragma once

#include "core.h"
#include "core/container/array.h"

struct QueueSubmitInfo;

#define MAX_COMMAND_BUFFERS 20

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

void make_vk_CommandPool(VkCommandPool&, Device& device, QueueType queue_type);
void make_CommandPool(CommandPool& pool, Device& device, QueueType queue_type, uint count);
void alloc_CommandBuffers(VkDevice device, VkCommandPool, uint count, VkCommandBuffer* result);
void destroy_CommandPool(CommandPool& pool);

//call once queue has completed execution
void completed_frame(CommandPool& pool, uint frame_index);

void begin_frame(CommandPool& pool, uint frame_index);
void submit_cmd_buffers(CommandPool& pool, QueueSubmitInfo&);
void end_frame(CommandPool& pool);

VkCommandBuffer begin_recording(CommandPool& pool);
void end_recording(CommandPool& pool, VkCommandBuffer cmd_buffer);
