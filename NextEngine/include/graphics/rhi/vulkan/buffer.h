#pragma once

#include "core.h"
#include "core/container/array.h"
#include "graphics/rhi/buffer.h"

struct VertexStreaming;
struct QueueSubmitInfo;
struct Device;

//StagingQueue is internally sychronized 
//in terms of keeping track of the current frame index
//and which command buffers have executed 
//however it is still invalid to submit commands on different threads.
//Limitations: The current design makes most sense if 
//Dedicated transfer operations 
//command buffer for that specific queue
struct StagingQueue {
	VkDevice device;
	VkPhysicalDevice physical_device;
	VkCommandPool command_pool;
	VkQueue queue;
	int queue_family;
	int dst_queue_family;

	VkCommandBuffer cmd_buffers[MAX_FRAMES_IN_FLIGHT];
	VkFence completed_transfer[MAX_FRAMES_IN_FLIGHT];
	
	VkSemaphore wait_on_transfer;
	VkSemaphore wait_on_resource_in_use;
	uint value_wait_on_transfer;
	uint value_wait_on_resource_in_use;

	uint frame_index;
	bool recording;
};

void transfer_queue_dependencies(StagingQueue&, QueueSubmitInfo&, uint transfer_frame_index);

using ArrayVertexInputs = array<20, VkVertexInputAttributeDescription>;
using ArrayVertexBindings = array<2, VkVertexInputBindingDescription>;

//todo tommorow optimization performing merging
//todo VkBuffer can be merged for different Layouts
struct VertexArena {
	u64 vertex_capacity;
	u64 index_capacity;
	u64 base_vertex_offset;
	u64 base_index_offset;
	u64 vertex_offset;
	u64 index_offset;
	u64 vertex_offset_start_of_frame;
	u64 index_offset_start_of_frame;
};

struct LayoutVertexInputs {
	ArrayVertexInputs input_desc;
	VkVertexInputBindingDescription binding_desc;
};

struct HostVisibleBuffer {
	VkBuffer buffer;
	VkDeviceMemory buffer_memory;
	int capacity;
	void* mapped;
};

//Currently this will allocate 4 device memory chunks
//todo perf: merge into one big buffer
//- staging buffer for the vertex data
//- device local vertex data
//- device local instance data
//- host visible instance data 

//it might make sense to split this
//as instance allocation is quite different to vertex buffers

struct VertexLayouts {
	LayoutVertexInputs vertex_layouts[VERTEX_LAYOUT_MAX] = {};
	LayoutVertexInputs instance_layouts[VERTEX_LAYOUT_MAX][INSTANCE_LAYOUT_MAX] = {};
    uint vertex_layout_count = VERTEX_LAYOUT_COUNT;
    uint instance_layout_count = INSTANCE_LAYOUT_COUNT;
};

struct VertexStreaming {
	StagingQueue& staging_queue;
	LayoutVertexInputs* layouts;

	VkDevice device;
	VkPhysicalDevice physical_device;

	HostVisibleBuffer staging_buffer;
	uint staging_offset;

	VkBuffer vertex_buffer;
	VkBuffer index_buffer;
	VkDeviceMemory vertex_buffer_memory;
	VkDeviceMemory index_buffer_memory;
	VertexArena arenas[VERTEX_LAYOUT_COUNT] = {};
};

struct InstanceAllocator {
	VkDevice device;
	VkPhysicalDevice physical_device;

	using Layouts = LayoutVertexInputs[VERTEX_LAYOUT_MAX][INSTANCE_LAYOUT_MAX];

	Layouts* layouts;

	uint frame_index;
	HostVisibleBuffer instance_memory;

	Arena arenas[MAX_FRAMES_IN_FLIGHT][INSTANCE_LAYOUT_COUNT];
};

struct UBOAllocator {
	VkDevice device;
	VkPhysicalDevice physical_device;
	uint alignment;

	HostVisibleBuffer ubo_memory;
	Arena arenas[UBO_UPDATE_MODE_COUNT];
};

VkBuffer get_buffer(buffer_handle);
VkDeviceMemory get_device_memory(device_memory_handle);

//CREATION AND DESTRUCTION
StagingQueue make_StagingQueue(VkDevice device, VkPhysicalDevice physical_device, VkCommandPool pool, VkQueue queue, int queue_family, int dst_queue_family);
uint begin_staging_cmds(StagingQueue& queue);
void end_staging_cmds(StagingQueue& queue);
void destroy_StagingQueue(StagingQueue& staging);

void make_Layouts(VertexLayouts& layouts);

void make_VertexStreaming(VertexStreaming& vertex_streaming, VkDevice device, VkPhysicalDevice physical_device,
                          StagingQueue& queue, slice<LayoutVertexInputs> layouts, u64* vertices_size_per_layout, u64*
                          indices_size_per_layout, u64 staging);
void destroy_VertexStreaming(VertexStreaming& vertex_streaming);

void make_InstanceAllocator(InstanceAllocator&, VkDevice device, VkPhysicalDevice physical_device, InstanceAllocator::Layouts*, u64* instance_size_per_layout);
void destroy_InstanceAllocator(InstanceAllocator&);

void make_UBOAllocator(UBOAllocator& self, Device& device, u64* backing);
void destroy_UBOAllocator(UBOAllocator&);
UBOBuffer alloc_ubo_buffer(UBOAllocator& self, uint size);
VertexBuffer alloc_vertex_buffer(VertexStreaming& self, VertexLayout layout, u64 vertices_length, void* vertices, u64
indices_length, uint* indices);

uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t typeFilter, VkMemoryPropertyFlags properties);
void memcpy_Buffer(VkDevice device, VkDeviceMemory memory, void* buffer_data, u64 buffer_size, u64 offset = 0);
void make_Buffer(VkDevice device, VkPhysicalDevice physical_device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
void copy_Buffer(StagingQueue& staging_queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
//void make_StagedBuffer(StagingQueue& staging_queue, void* bufferData, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

void input_attributes(ArrayVertexInputs& vertex_inputs, slice<VertexAttrib> attribs, int binding);
ArrayVertexInputs input_attributes(VertexStreaming& self, VertexLayout layout);
VkVertexInputBindingDescription input_bindings(VertexStreaming& self, VertexLayout layout);
ArrayVertexInputs input_attributes(VertexLayouts& layouts, VertexLayout layout, InstanceLayout instance_layout);
ArrayVertexBindings input_bindings(VertexLayouts& layouts, VertexLayout layout, InstanceLayout instance_layout);

void fill_layout(LayoutVertexInputs& layout, VkVertexInputRate rate, slice<VertexAttrib> attribs, uint elem_size, uint binding);

HostVisibleBuffer make_HostVisibleBuffer(VkDevice device, VkPhysicalDevice physical_device, uint usage, u64 size);
void destroy_HostVisibleBuffer(VkDevice device, VkPhysicalDevice physical_device, HostVisibleBuffer&);
void map_buffer_memory(VkDevice device, HostVisibleBuffer& buffer_backing);
void unmap_buffer_memory(VkDevice device, HostVisibleBuffer& buffer_backing);

void begin_frame(InstanceAllocator& self, uint frame_index);
//void end_frame(InstanceAllocator& self);

void bind_vertex_buffer(VertexStreaming&, VkCommandBuffer cmd_buffer, VertexLayout v_layout);
void bind_instance_buffer(InstanceAllocator&, VkCommandBuffer cmd_buffer, InstanceLayout layout);

void begin_vertex_buffer_upload(VertexStreaming& self);
void end_vertex_buffer_upload(VertexStreaming& self);
void transfer_vertex_ownership(VertexStreaming& self, VkCommandBuffer cmd_buffer);

void staged_copy(VertexStreaming& allocator, VkBuffer dst, i64 offset, void* data, i64 size);