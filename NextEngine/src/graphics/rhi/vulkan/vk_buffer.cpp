#ifdef RENDER_API_VULKAN

#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/buffer.h"
#include <stdio.h>
#include "core/container/tvector.h"
#include "core/container/array.h"

//LAYOUTS
#include "graphics/assets/model.h"
#include "graphics/renderer/terrain.h"

//TRANSFER FRAME CAN BE LOOSELY TIED TO THE RENDER THREAD
//AND CAN SUBMIT TRANSFERS AT A COMPLETELY RATE
//AS IT IS USING A SEPERATE TRANSFER QUEUE
//HOWEVER THIS MUST BE SYCHRONIZED WITH THE GRAPHICS QUEUE

StagingQueue make_StagingQueue(VkDevice device, VkPhysicalDevice physical_device, VkCommandPool pool, VkQueue queue, int queue_family, int dst_queue_family) {
	StagingQueue result = {};
	result.queue = queue;
	result.queue_family = queue_family;
	result.dst_queue_family = dst_queue_family;
	result.device = device;
	result.physical_device = physical_device;
	result.command_pool = pool;

	

	result.wait_on_transfer = make_timeline_Semaphore(device);
	result.wait_on_resource_in_use = make_timeline_Semaphore(device);
	
	for (uint i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		result.completed_transfer[i] = make_Fence(device);
	}

	alloc_CommandBuffers(device, pool, MAX_FRAMES_IN_FLIGHT, result.cmd_buffers);	
	return result;
}


uint begin_staging_cmds(StagingQueue& queue) {
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; 

	vkWaitForFences(queue.device, 1, &queue.completed_transfer[queue.frame_index], true, INT_MAX);

	assert(!queue.recording);
	queue.recording = true;
	vkBeginCommandBuffer(queue.cmd_buffers[queue.frame_index], &beginInfo);

	return ++queue.value_wait_on_transfer;
}

void end_staging_cmds(StagingQueue& queue) {
	VkDevice device = queue.device;
	VkCommandBuffer cmd_buffer = queue.cmd_buffers[queue.frame_index];
	VkFence completed = queue.completed_transfer[queue.frame_index];

	vkEndCommandBuffer(cmd_buffer);

	vkWaitForFences(queue.device, 1, &completed, true, INT_MAX);

	int signal_value = queue.value_wait_on_transfer;

	QueueSubmitInfo info = {};
	info.completion_fence = completed;
	info.cmd_buffers.append(queue.cmd_buffers[queue.frame_index]);

	//uint64_t value;
	//vkGetSemaphoreCounterValue(device, queue.wait_on_transfer, &value);

	printf("SIGNALLING SEMAPHORE WITH CURRENT VALUE %i\n", signal_value);

	queue_signal_timeline_semaphore(info, queue.wait_on_transfer, signal_value);
	
	/*VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd_buffer;
	submitInfo.signalSemaphoreCount = MAX_FRAMES_IN_FLIGHT;
	submitInfo.pSignalSemaphores = queue.wait_on_transfer;*/
	
	vkResetFences(device, 1, &completed);

	queue_submit(device, queue.queue, info);

	queue.recording = false;

	queue.frame_index = (queue.frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
}

//queue.frame_index is the transfer frame, which can be independent of the frame count
//however this has to be sychronized properly
void transfer_queue_dependencies(StagingQueue& queue, QueueSubmitInfo& info, uint transfer_frame_index) {
	queue_wait_timeline_semaphore(info, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, queue.wait_on_transfer, transfer_frame_index);
}

void destroy_StagingQueue(StagingQueue& staging) {
	vkFreeCommandBuffers(staging.device, staging.command_pool, MAX_FRAMES_IN_FLIGHT, staging.cmd_buffers);
}

uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physical_device, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) return i;
	}

	throw "Failed to find suitable memory type";
}

void make_Buffer(VkDevice device, VkPhysicalDevice physical_device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw "failed to make buffer!";
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = find_memory_type(physical_device, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		throw "failed to allocate buffer memory!";
	}

	vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void copy_Buffer(StagingQueue& staging_queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
	VkBufferCopy copyRegion = {};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;

	vkCmdCopyBuffer(staging_queue.cmd_buffers[staging_queue.frame_index], srcBuffer, dstBuffer, 1, &copyRegion);
	//transferQueue.cmdsCopyBuffer.append({srcBuffer, dstBuffer, copyRegion});
}

void memcpy_Buffer(VkDevice device, VkDeviceMemory memory, void* buffer_data, u64 buffer_size, u64 offset) {
	void* data;
	vkMapMemory(device, memory, 0, buffer_size, 0, &data);
	memcpy((char*)data + offset, buffer_data, (size_t)buffer_size);
	vkUnmapMemory(device, memory);
}

void staged_copy(VertexStreaming& allocator, VkBuffer dst, i64 offset, void* data, i64 size) {
	HostVisibleBuffer& staging_buffer = allocator.staging_buffer;
	
	char* mapped = (char*)allocator.staging_buffer.mapped;
	memcpy(mapped + allocator.staging_offset, data, size);
	
	VkBufferCopy copyRegion = {};
	copyRegion.srcOffset = allocator.staging_offset;
	copyRegion.dstOffset = offset;
	copyRegion.size = size;

	vkCmdCopyBuffer(allocator.staging_queue.cmd_buffers[allocator.staging_queue.frame_index], staging_buffer.buffer, dst, 1, &copyRegion);

	allocator.staging_offset += size;
	assert(allocator.staging_offset <= allocator.staging_buffer.capacity);
}


void input_attributes(ArrayVertexInputs& vertex_inputs, slice<VertexAttrib> attribs, int binding) {

	for (int i = 0; i < attribs.length; i++) {
		VertexAttrib& attrib = attribs[i];
		VkVertexInputAttributeDescription input_attribute_desc = {};
		input_attribute_desc.binding = binding;
		input_attribute_desc.location = vertex_inputs.length;
		input_attribute_desc.offset = attrib.offset;

		if (attrib.type == VertexAttrib::Float) {
			VkFormat format[4] = { VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT };
			input_attribute_desc.format = format[attrib.length - 1];
		}
		else if (attrib.type == VertexAttrib::Int) {
			VkFormat format[4] = { VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32B32_SINT, VK_FORMAT_R32G32B32A32_SINT };
			input_attribute_desc.format = format[attrib.length - 1];
		}

		else if (attrib.type == VertexAttrib::Unorm) {
			VkFormat format[4] = { VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };
			input_attribute_desc.format = format[attrib.length - 1];
		}
        else {
            printf("Unexpected format!");
            abort();
        }

		vertex_inputs.append(input_attribute_desc);
	}
}

ArrayVertexInputs input_attributes(VertexStreaming& self, VertexLayout layout) {
	return self.layouts[layout].input_desc;	
}

VkVertexInputBindingDescription input_bindings(VertexStreaming& self, VertexLayout layout) {
	return self.layouts[layout].binding_desc; 
}

ArrayVertexInputs input_attributes(VertexLayouts& layouts, VertexLayout layout, InstanceLayout instance_layout) {
	return layouts.instance_layouts[layout][instance_layout].input_desc;
}

ArrayVertexBindings input_bindings(VertexLayouts& layouts, VertexLayout layout, InstanceLayout instance_layout) {
    if (instance_layout == INSTANCE_LAYOUT_NONE) {
        return {layouts.vertex_layouts[layout].binding_desc};
    }
    return {
		layouts.vertex_layouts[layout].binding_desc,
		layouts.instance_layouts[layout][instance_layout].binding_desc
	};
}

void fill_layout(LayoutVertexInputs& layout, VkVertexInputRate rate, slice<VertexAttrib> attribs, uint elem_size, uint binding) {
	VkVertexInputBindingDescription vertex_binding_description = {};
	vertex_binding_description.binding = binding;
	vertex_binding_description.stride = elem_size;
	vertex_binding_description.inputRate = rate;

	layout.binding_desc = vertex_binding_description;
	input_attributes(layout.input_desc, attribs, 0);
}

void fill_vertex_layouts(VertexLayouts& layouts, VertexLayout vertex_layout, VertexLayoutDesc& vertex_desc) {
	LayoutVertexInputs& layout = layouts.vertex_layouts[vertex_layout];
	fill_layout(layout, VK_VERTEX_INPUT_RATE_VERTEX, vertex_desc.attribs, vertex_desc.elem_size, 0);
}

void fill_instance_layouts(VertexLayouts& layouts, VertexLayout vertex_layout, InstanceLayout instance_layout, VertexLayoutDesc& vert_desc, InstanceLayoutDesc& instance_desc) {
	LayoutVertexInputs& layout = layouts.instance_layouts[vertex_layout][instance_layout];
	fill_layout(layout, VK_VERTEX_INPUT_RATE_INSTANCE, vert_desc.attribs, instance_desc.elem_size, 1);
	input_attributes(layout.input_desc, instance_desc.attribs, 1);
}

void alloc_instance_layout_allocator(InstanceAllocator& m, uint frame, InstanceLayout instance_layout, uint capacity, uint* offset) {
	Arena& allocator = m.arenas[m.frame_index][instance_layout];
	allocator.capacity = capacity;
	allocator.base_offset = *offset;
	*offset += capacity;
}

//Caller is responsible for batching alloc vertex buffer, into reasonable large chunks
//otherwise this will generate unnecessary vkCmdCopy commands,
//buffer allocator could batch these, but that's more complicated

VertexBuffer alloc_vertex_buffer(VertexStreaming& self, VertexLayout layout, int vertices_length, void* vertices, int indices_length, uint* indices) {		
	VertexArena& allocator = self.arenas[layout];
	int vert_size = self.layouts[layout].binding_desc.stride;
	int index_size = sizeof(uint);
	int vertices_size = vertices_length * vert_size;
	int indices_size = indices_length * index_size;

	log("UPLOADING VERTEX BUFFER Vertex: (", vert_size, ") ", vertices_length, " ", indices_length, ", size ",  vertices_size, " ", indices_size, "\n");

	assert(allocator.vertex_capacity >= allocator.vertex_offset + vertices_size);
	assert(allocator.index_capacity >= allocator.index_offset + indices_length * sizeof(uint));
		
	VertexBuffer buffer;
	buffer.layout = layout;
	buffer.length = indices_length;
	buffer.vertex_capacity = vertices_length;
	buffer.instance_capacity = indices_length;

	int vertex_offset = allocator.vertex_offset;
	int index_offset  = allocator.index_offset;
	
	buffer.vertex_base = vertex_offset / vert_size;
	buffer.index_base = index_offset / index_size;	
	
	allocator.vertex_offset += vertices_size;
	allocator.index_offset += indices_size;

	printf("OFFSET vertex : %i, index : %i\n", vertex_offset, index_offset);

	if (vertices) staged_copy(self, self.vertex_buffer, vertex_offset, vertices, vertices_size);
	if (indices) staged_copy(self, self.index_buffer, index_offset, indices, indices_size);
	
	return buffer;
}

void map_buffer_memory(VkDevice device, HostVisibleBuffer& buffer_backing) {
	assert(buffer_backing.mapped == NULL);
	vkMapMemory(device, buffer_backing.buffer_memory, 0, buffer_backing.capacity, 0, &buffer_backing.mapped);
}

void unmap_buffer_memory(VkDevice device, HostVisibleBuffer& buffer_backing) {
	if (buffer_backing.mapped) {
		vkUnmapMemory(device, buffer_backing.buffer_memory);
	}
}

void transfer_vertex_ownership(VertexStreaming& self, VkCommandBuffer cmd_buffer) {
	StagingQueue& staging = self.staging_queue;

	VkBufferMemoryBarrier buffer_barriers[2] = {};

	VertexArena& vertex_buffer = self.arenas[0];

	VkBufferMemoryBarrier& vertex_barrier = buffer_barriers[0];
	vertex_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	vertex_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	vertex_barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	vertex_barrier.srcQueueFamilyIndex = staging.queue_family;
	vertex_barrier.dstQueueFamilyIndex = staging.dst_queue_family;
	vertex_barrier.buffer = self.vertex_buffer;

	//todo urgent: this should be refactored into a loop, which has to execute per vertex buffer layout
	//as data uploads are only continous within each vertex buffer layout chunk
	vertex_barrier.size = vertex_buffer.vertex_offset - self.arenas->vertex_offset_start_of_frame;
	vertex_barrier.offset = vertex_buffer.vertex_offset;

	VkBufferMemoryBarrier& index_barrier = buffer_barriers[1];
	index_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	index_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	index_barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	index_barrier.srcQueueFamilyIndex = staging.queue_family;
	index_barrier.dstQueueFamilyIndex = staging.dst_queue_family;
	index_barrier.buffer = self.index_buffer;
	index_barrier.size = vertex_buffer.index_offset - vertex_buffer.index_offset_start_of_frame;
	index_barrier.offset = vertex_buffer.index_offset;

	if (vertex_barrier.size == 0 && index_barrier.size == 0) {
		return;
	}

	//todo implement ownership transfer back

	vkCmdPipelineBarrier(cmd_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
		0,
		0, nullptr,
		2, buffer_barriers,
		0, nullptr
	);

	printf("PIPELINE BARRIER\n");
}

void begin_frame(InstanceAllocator& self, uint frame_index) {
	//Since the instance data is no longer being used (all the frames have completed)
	//we can reset it

	for (int i = 0; i < INSTANCE_LAYOUT_COUNT; i++) {
		self.arenas[frame_index][i].offset = 0;
	}

	self.frame_index = frame_index;
}

void begin_vertex_buffer_upload(VertexStreaming& self) {
	
}

void end_vertex_buffer_upload(VertexStreaming& self) {
	//todo perf: in terms of parrelelism this is not great.
	//none of the next frame can render before waiting on the transfer commands
	//an intelligent solution would be to split command buffers up depending on whether they
	//depend on certain new transfers, and differentiate between texture and buffer transfers

	transfer_vertex_ownership(self, self.staging_queue.cmd_buffers[self.staging_queue.frame_index]);

	//todo this will only work as long as there is 1, vertex layout
	self.staging_offset = 0;
	self.arenas->index_offset_start_of_frame = self.arenas->index_offset;
	self.arenas->vertex_offset_start_of_frame = self.arenas->vertex_offset;
}

/*
void upload_data(BufferAllocator& self, InstanceBuffer& buffer, int length, void* data) {
	InstanceBufferAllocator& allocator = self.instance_buffers[self.frame_index_index][buffer.layout];
	i64 size = allocator.elem_size * length;
	i64 offset = allocator.elem_size * buffer.base;
		
	assert(length <= buffer.capacity);		
	buffer.length = length;

	memcpy_Buffer(self.device, allocator.buffer_memory, data, size, offset);
	//staged_copy(self.device, self.staging, allocator.buffer, offset, data, size);
}
*/

InstanceBuffer alloc_instance_buffer(InstanceAllocator& self, InstanceLayout layout, int length, void** data) {
	Arena& allocator = self.arenas[self.frame_index][layout];
	int elem = (*self.layouts)[0][layout].binding_desc.stride;
	int size = length * elem;

	assert(allocator.capacity >= allocator.offset + size);

	*data = (char*)self.instance_memory.mapped + allocator.base_offset + allocator.offset;

	InstanceBuffer buffer;
	buffer.layout = layout;
	buffer.base = allocator.offset / elem;
	buffer.capacity = length;
	buffer.length = length;

	allocator.offset += size;

	return buffer;
}

UBOBuffer alloc_ubo_buffer(UBOAllocator& self, uint size, UBOUpdateFlags update_mode) {
	//update_mode = 0;
	
	Arena& arena = self.arenas[0]; //update_mode];

	UBOBuffer buffer;
	buffer.flags = update_mode;
	buffer.offset = arena.base_offset + arena.offset;
	buffer.size = size;

	buffer.offset = align_offset(buffer.offset, self.alignment);
	arena.offset = buffer.offset - arena.base_offset + size;

	assert(arena.offset <= arena.capacity);
	assert(buffer.offset % self.alignment == 0);

	return buffer;
}

void memcpy_ubo_buffer(UBOAllocator& self, UBOBuffer& buffer, uint size, void* data) {
	HostVisibleBuffer visible = self.ubo_memory;
	memcpy((char*)visible.mapped + buffer.offset, data, size);
	
	//todo could optimize by setting different flags, such as making large ubo buffers that never change device local
	//if (update_mode == UBO_MAP_UNMAP) {
	//	memcpy_Buffer(self.device, visible.buffer_memory, data, size, buffer.offset);
	//}
	//else if (update_mode == UBO_PERMANENT_MAP) {
	//	memcpy(visible.mapped, data, size);
	//}
}

void bind_vertex_buffer(VertexStreaming& self, VkCommandBuffer cmd_buffer, VertexLayout v_layout) {
	VertexArena& vertex_buffer = self.arenas[v_layout];

	VkDeviceSize offset = vertex_buffer.base_vertex_offset;
	VkBuffer buffers = self.vertex_buffer;

	vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &self.vertex_buffer, &offset);
	vkCmdBindIndexBuffer(cmd_buffer, self.index_buffer, vertex_buffer.base_index_offset, VK_INDEX_TYPE_UINT32);
}

void bind_instance_buffer(InstanceAllocator& self, VkCommandBuffer cmd_buffer, InstanceLayout layout) {
	Arena& instance_buffer = self.arenas[self.frame_index][layout];

	VkDeviceSize offset = instance_buffer.base_offset;
	vkCmdBindVertexBuffers(cmd_buffer, 1, 1, &self.instance_memory.buffer, &offset);
}

CPUVisibleBuffer alloc_cpu_visibile_buffer(u64 size, BufferUsageFlags usage) {
    CPUVisibleBuffer buffer;
    buffer.buffer = alloc_buffer_and_memory(size, usage, &buffer.memory, MEMORY_CPU_WRITEABLE);
    buffer.mapped = map_memory(buffer.memory, 0, size);
    buffer.capacity = size;
    
    return buffer;
}

void dealloc_buffer(buffer_handle handle) {
    VkBuffer buffer = get_buffer(handle);
    vkDestroyBuffer(rhi.device, buffer, 0);
}

void dealloc_cpu_visibile_buffer(CPUVisibleBuffer& buffer) {
    unmap_memory(buffer.memory);
    dealloc_buffer(buffer.buffer);
    dealloc_device_memory(buffer.memory);
}


HostVisibleBuffer make_HostVisibleBuffer(VkDevice device, VkPhysicalDevice physical_device, uint usage, uint size) {
	HostVisibleBuffer buffer = {};
	buffer.capacity = size;
	make_Buffer(device, physical_device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buffer.buffer, buffer.buffer_memory);
	return buffer;
}

void make_Layouts(VertexLayouts& layouts) {
	VertexLayoutDesc vertex_layout_desc;
	vertex_layout_desc.elem_size = sizeof(Vertex);
	vertex_layout_desc.attribs = {
		{ 3, VertexAttrib::Float, offsetof(Vertex, position) },
		{ 3, VertexAttrib::Float, offsetof(Vertex, normal) },
		{ 2, VertexAttrib::Float, offsetof(Vertex, tex_coord) },
		{ 3, VertexAttrib::Float, offsetof(Vertex, tangent) },
		{ 3, VertexAttrib::Float, offsetof(Vertex, bitangent) }
	};

	InstanceLayoutDesc layout_desc_mat4x4;
	layout_desc_mat4x4.elem_size = sizeof(glm::mat4);
	layout_desc_mat4x4.attribs = {
		{ 4, VertexAttrib::Float, sizeof(float) * 0 },
		{ 4, VertexAttrib::Float, sizeof(float) * 4 },
		{ 4, VertexAttrib::Float, sizeof(float) * 8 },
		{ 4, VertexAttrib::Float, sizeof(float) * 12 }
	};

	InstanceLayoutDesc layout_desc_terrain_chunk;
	layout_desc_terrain_chunk.elem_size = sizeof(ChunkInfo);
	layout_desc_terrain_chunk.attribs = layout_desc_mat4x4.attribs;
	layout_desc_terrain_chunk.attribs.append({ 2, VertexAttrib::Float, offsetof(ChunkInfo, displacement_offset) });
	layout_desc_terrain_chunk.attribs.append({ 1, VertexAttrib::Float, offsetof(ChunkInfo, lod) });
	layout_desc_terrain_chunk.attribs.append({ 1, VertexAttrib::Float, offsetof(ChunkInfo, edge_lod) });

	fill_vertex_layouts(layouts, VERTEX_LAYOUT_DEFAULT, vertex_layout_desc);

	fill_instance_layouts(layouts, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4, vertex_layout_desc, layout_desc_mat4x4);
	fill_instance_layouts(layouts, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_TERRAIN_CHUNK, vertex_layout_desc, layout_desc_terrain_chunk);
}

void make_InstanceAllocator(InstanceAllocator& self, VkDevice device, VkPhysicalDevice physical_device, InstanceAllocator::Layouts* layouts, u64* instance_size_per_layout) {
	self = {};
	self.device = device;
	self.physical_device = physical_device;
	self.layouts = layouts;

	uint instances_offset = 0;

	for (int layout = 0; layout < INSTANCE_LAYOUT_COUNT; layout++) {
		u64 capacity = instance_size_per_layout[layout];
		
		for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
			Arena& allocator = self.arenas[frame][layout];
			allocator.capacity = capacity;
			allocator.base_offset = instances_offset;
			instances_offset += capacity;
		}
	}

	self.instance_memory = make_HostVisibleBuffer(device, self.physical_device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, instances_offset * MAX_FRAMES_IN_FLIGHT);
	map_buffer_memory(self.device, self.instance_memory);
}

void make_UBOAllocator(UBOAllocator& self, Device& device, u64* backing) {
	self = {};
	self.device = device.device;
	self.physical_device = device.physical_device;
	self.alignment = device.device_limits.minUniformBufferOffsetAlignment;

	u64 ubo_offset = 0;

	for (int arena = 0; arena < UBO_UPDATE_MODE_COUNT; arena++) {
		Arena& allocator = self.arenas[arena];
		allocator.capacity = backing[arena];
		allocator.base_offset = ubo_offset;
		
		ubo_offset += backing[arena];
	}

	self.ubo_memory = make_HostVisibleBuffer(device, self.physical_device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, ubo_offset);
	map_buffer_memory(self.device, self.ubo_memory);
}

void make_VertexStreaming(VertexStreaming& self, VkDevice device, VkPhysicalDevice physical_device, StagingQueue& queue, LayoutVertexInputs* layouts, u64* vertices_size_per_layout, u64* indices_size_per_layout) {
	self.device = device;
	self.physical_device = physical_device;
	self.staging_queue = queue;
	self.layouts = layouts;
	
	uint vertex_offset = 0;
	uint index_offset = 0;

	for (int layout = 0; layout < VERTEX_LAYOUT_COUNT; layout++) {
		VertexArena& allocator = self.arenas[layout];
		allocator.vertex_capacity = vertices_size_per_layout[layout];
		allocator.index_capacity = indices_size_per_layout[layout];
		allocator.base_vertex_offset = vertex_offset;
		allocator.base_index_offset = index_offset;

		vertex_offset += allocator.vertex_capacity;
		index_offset += allocator.index_capacity;
	}

	//Allocate Buffers
	self.staging_buffer = make_HostVisibleBuffer(device, self.physical_device, 0, mb(200));

	make_Buffer(device, physical_device, vertex_offset, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, self.vertex_buffer, self.vertex_buffer_memory);
	make_Buffer(device, physical_device, index_offset, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, self.index_buffer, self.index_buffer_memory);

	map_buffer_memory(self.device, self.staging_buffer);
}

void destroy_VertexStreaming(VertexStreaming& self) {
	VkDevice device = self.device;

	vkDestroyBuffer(device, self.staging_buffer.buffer, nullptr);
	vkFreeMemory(device, self.staging_buffer.buffer_memory, nullptr);

	vkDestroyBuffer(device, self.vertex_buffer, nullptr);
	vkFreeMemory(device, self.vertex_buffer_memory, nullptr);
}

void destroy_InstanceAllocator(InstanceAllocator& self) {
	VkDevice device = rhi.device;
	
	unmap_buffer_memory(device, self.instance_memory);

	vkDestroyBuffer(device, self.instance_memory.buffer, nullptr);
	vkFreeMemory(device, self.instance_memory.buffer_memory, nullptr);
}

//GLOBAL RHI API
struct HandlesToVK {
    VkBuffer buffer[100];
    VkDeviceMemory memory[100];
    uint buffer_count;
    uint memory_count;
};

static HandlesToVK handles;

VkDeviceMemory get_device_memory(device_memory_handle handle) { return handles.memory[handle.id - 1]; }
VkBuffer get_buffer(buffer_handle handle) { return handles.buffer[handle.id - 1]; }

device_memory_handle alloc_device_memory(uint size, uint resource_type, DeviceMemoryFlags flags) {
    VkMemoryPropertyFlags properties;
    if (flags == MEMORY_GPU_NATIVE) properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (flags == MEMORY_CPU_READABLE) properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    if (flags == MEMORY_CPU_WRITEABLE) properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = size;
    allocInfo.memoryTypeIndex = find_memory_type(rhi.device, resource_type, properties);

    uint index = handles.memory_count++;

    if (vkAllocateMemory(rhi.device, &allocInfo, nullptr, handles.memory + index) != VK_SUCCESS) {
        fprintf(stderr, "failed to allocate buffer memory!");
        return {INVALID_HANDLE};
    }

    return {index + 1};
}


void dealloc_device_memory(device_memory_handle handle) {
    //todo
}

void* map_memory(device_memory_handle handle, u64 offset, u64 size) {
    void* mapped = nullptr;
    vkMapMemory(rhi.device, get_device_memory(handle), offset, size, 0, &mapped);
    return mapped;
}

void unmap_memory(device_memory_handle handle) {
    vkUnmapMemory(rhi.device, get_device_memory(handle));
}

MemoryRequirements query_buffer_memory_requirements(buffer_handle buffer) {
    VkMemoryRequirements vk_requirements;
    vkGetBufferMemoryRequirements(rhi.device, get_buffer(buffer), &vk_requirements);
    
    MemoryRequirements result;
    result.size = vk_requirements.size;
    result.resource_type = vk_requirements.memoryTypeBits;
    result.alignment = vk_requirements.alignment;
    
    return result;
}

buffer_handle alloc_buffer(u64 size, BufferUsageFlags usage) {
    VkBufferUsageFlags vk_usage = 0;
    if (usage == BUFFER_VERTEX) vk_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (usage == BUFFER_INDEX) vk_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (usage == BUFFER_UBO) vk_usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = vk_usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    uint index = handles.buffer_count++;
    
    if (vkCreateBuffer(rhi.device, &buffer_info, nullptr, handles.buffer + index) != VK_SUCCESS) {
        throw "failed to make buffer!";
    }
    
    return {index + 1};
}

void bind_buffer_memory(buffer_handle buffer, device_memory_handle memory, u64 offset) {
    vkBindBufferMemory(rhi.device, get_buffer(buffer), get_device_memory(memory), offset);
}

buffer_handle alloc_buffer(u64 size, BufferUsageFlags usage, device_memory_handle memory, u64 offset) {
    buffer_handle handle = alloc_buffer(size, usage);
    bind_buffer_memory(handle, memory, offset);
    return handle;
}

buffer_handle alloc_buffer_and_memory(u64 size, BufferUsageFlags usage, device_memory_handle* memory, DeviceMemoryFlags memory_flags) {
    buffer_handle handle = alloc_buffer(size, usage);
    
    MemoryRequirements requirements = query_buffer_memory_requirements(handle);
    *memory = alloc_device_memory(requirements.size, requirements.resource_type, memory_flags);
    
    bind_buffer_memory(handle, *memory, 0);
    return handle;
}

VertexLayout register_vertex_layout(VertexLayoutDesc& desc) {
    VertexLayout vertex_layout = (VertexLayout)(rhi.vertex_layouts.vertex_layout_count++);
    
    InstanceLayoutDesc layout_desc_none = {};
    
    fill_vertex_layouts(rhi.vertex_layouts, vertex_layout, desc);
    fill_instance_layouts(rhi.vertex_layouts, vertex_layout, INSTANCE_LAYOUT_NONE, desc, layout_desc_none);
    
    return vertex_layout;
}

VertexBuffer alloc_vertex_buffer(VertexLayout vertex_layout, int vertices_length, void* vertices, int indices_length, uint* indices) {		
	return alloc_vertex_buffer(rhi.vertex_streaming, vertex_layout, vertices_length, vertices, indices_length, indices);
}

//in theory allocating an instance buffer does not depend on VertexLayout
InstanceBuffer frame_alloc_instance_buffer(InstanceLayout instance_layout, uint length, void** ptr) {
	return alloc_instance_buffer(render_thread.instance_allocator, instance_layout, length, ptr);
}

UBOBuffer alloc_ubo_buffer(uint size, UBOUpdateFlags flags) {
	return alloc_ubo_buffer(rhi.ubo_allocator, size, flags);
}

void memcpy_ubo_buffer(UBOBuffer& buffer, uint size, void* data) {
	memcpy_ubo_buffer(rhi.ubo_allocator, buffer, size, data);
}



#endif
