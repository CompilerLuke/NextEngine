#pragma once

#include <glm/mat4x4.hpp>
#include "core.h"
#include "device.h"
#include "swapchain.h"
#include "buffer.h"
#include "pipeline.h"
#include "texture.h"
#include "command_buffer.h"
#include "shader_access.h"
#include "material.h"

struct RenderThreadResources {
	CommandPool command_pool;
	InstanceAllocator instance_allocator;
};


struct DestructionJob {
	void* data;
	void(*func)(void*);
};

struct RHI {
	VulkanDesc desc;
	Device device;
	Swapchain swapchain;
	Window* window;
	
	shaderc_compiler_t shader_compiler;
	VertexLayouts vertex_layouts;
	VertexStreaming vertex_streaming;
	TextureAllocator texture_allocator;
	CommandPool background_graphics;

	UBOAllocator ubo_allocator;
	PipelineCache pipeline_cache;
	StagingQueue staging_queue;
	VkCommandPool transfer_cmd_pool;
	DescriptorPool descriptor_pool;
	MaterialAllocator material_allocator;

	uint waiting_on_transfer_frame;
	uint frame_index;

	vector<DestructionJob> queued_for_destruction[MAX_FRAMES_IN_FLIGHT];

	RHI();
};

extern thread_local RenderThreadResources render_thread;
extern RHI rhi;

ENGINE_API void begin_gpu_upload();
ENGINE_API void end_gpu_upload();

struct Window;
struct RHI;

void make_RHI(const VulkanDesc&, Window&);
void destroy_RHI();