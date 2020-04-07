#include "stdafx.h"
#include <glad/glad.h>
#include "graphics/rhi/buffer.h"
#include "graphics/rhi/vulkan/buffer.h"
#include "graphics/rhi/vulkan/rhi.h"
#include "graphics/rhi/vulkan/vulkan.h"
#include <stdio.h>
#include "core/container/tvector.h"
#include "core/container/array.h"

//LAYOUTS
#include "graphics/assets/model.h"
#include "graphics/renderer/terrain.h"

#ifdef RENDER_API_VULKAN

StagingQueue make_StagingQueue(VkDevice device, VkPhysicalDevice physical_device, VkCommandPool pool, VkQueue queue) {
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = pool;
	allocInfo.commandBufferCount = 1;

	//TODO make CommandBuffer Pool
	StagingQueue result;
	result.queue = queue;
	result.device = device;
	result.physical_device = physical_device;
	result.command_pool = pool;
	vkAllocateCommandBuffers(device, &allocInfo, &result.cmd_buffer);
	return result;
}

void begin_staging_cmds(StagingQueue& queue) {
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(queue.cmd_buffer, &beginInfo);
}

void clear_StagingQueue(StagingQueue& queue) {
	queue.destroyBuffers.clear();
	queue.destroyDeviceMemory.clear();
}

void end_staging_cmds(StagingQueue& queue) {
	VkDevice device = queue.device;
	
	vkEndCommandBuffer(queue.cmd_buffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &queue.cmd_buffer;

	VK_CHECK(vkQueueSubmit(queue.queue, 1, &submitInfo, VK_NULL_HANDLE));
	vkQueueWaitIdle(queue.queue);

	for (VkBuffer buffer : queue.destroyBuffers) {
		vkDestroyBuffer(device, buffer, nullptr);
	}
	for (VkDeviceMemory memory : queue.destroyDeviceMemory) {
		vkFreeMemory(device, memory, nullptr);
	}

	clear_StagingQueue(queue);
}

void destroy_StagingQueue(StagingQueue& staging) {
	vkFreeCommandBuffers(staging.device, staging.command_pool, 1, &staging.cmd_buffer);
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

	vkCmdCopyBuffer(staging_queue.cmd_buffer, srcBuffer, dstBuffer, 1, &copyRegion);
	//transferQueue.cmdsCopyBuffer.append({srcBuffer, dstBuffer, copyRegion});
}

void memcpy_Buffer(VkDevice device, VkDeviceMemory memory, void* buffer_data, u64 buffer_size, u64 offset = 0) {
	void* data;
	vkMapMemory(device, memory, 0, buffer_size, 0, &data);
	memcpy((char*)data + offset, buffer_data, (size_t)buffer_size);
	vkUnmapMemory(device, memory);
}

void make_StagedBuffer(StagingQueue& staging_queue, void* bufferData, VkDeviceSize bufferSize, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
	VkDevice device = staging_queue.device;
	VkPhysicalDevice physical_device = staging_queue.physical_device;
	
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	make_Buffer(device, physical_device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	memcpy_Buffer(device, stagingBufferMemory, bufferData, bufferSize);

	make_Buffer(device, physical_device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, bufferMemory);

	copy_Buffer(staging_queue, stagingBuffer, buffer, bufferSize);

	staging_queue.destroyBuffers.append(stagingBuffer);
	staging_queue.destroyDeviceMemory.append(stagingBufferMemory);

	//vkDestroyBuffer(device, stagingBuffer, nullptr);
	//vkFreeMemory(device, stagingBufferMemory, nullptr);
}

struct VertexBufferAllocator {
	VertexLayout layout;
	VertexLayout vertex_layout;
	VkBuffer vbo;
	VkBuffer ebo;
	VkDeviceMemory vbo_memory;
	VkDeviceMemory ebo_memory;
	ArrayVertexInputs input_desc;
	VkVertexInputBindingDescription binding_desc;
	int vertex_capacity;
	int index_capacity;
	int vertex_offset;
	int index_offset;
	int elem_size;
};

struct InstanceBufferAllocator {
	InstanceLayout layout;
	VertexLayout vertex_layout;
	VkBuffer buffer;
	VkDeviceMemory buffer_memory;
	ArrayVertexInputs input_desc;
	VkVertexInputBindingDescription binding_desc;
	int capacity;
	int offset;
	int elem_size;
};

struct VertexLayoutDesc {
	VertexLayout layout;
	vector<VertexAttrib> attribs;
	int elem_size;
	int vertices_size;
	int indices_size;
};

struct InstanceLayoutDesc {
	InstanceLayout layout;
	vector<VertexAttrib> attribs;
	int elem_size;
	int size;
};

//BUFFER MANAGER STATE
struct StagingBufferAllocator {
	StagingQueue queue;
	VkBuffer buffer;
	VkDeviceMemory buffer_memory;
	int capacity;
	int offset;
};

struct BufferAllocator {
	VkDevice device;
	VkPhysicalDevice physical_device;
	StagingBufferAllocator staging = {};
	VertexBufferAllocator vertex_buffers[VERTEX_LAYOUT_COUNT] = {};
	InstanceBufferAllocator instance_buffers[INSTANCE_LAYOUT_COUNT] = {};
};

void staged_copy(VkDevice device, StagingBufferAllocator& staging, VkBuffer dst, i64 offset, void* data, i64 size) {
	memcpy_Buffer(device, staging.buffer_memory, data, size, staging.offset);

	VkBufferCopy copyRegion = {};
	copyRegion.srcOffset = staging.offset;
	copyRegion.dstOffset = offset;
	copyRegion.size = size;

	vkCmdCopyBuffer(staging.queue.cmd_buffer, staging.buffer, dst, 1, &copyRegion);

	staging.offset += size;
	assert(staging.offset <= staging.capacity);
}

void input_attributes(ArrayVertexInputs& vertex_inputs, slice<VertexAttrib> attribs, int binding) {

	for (int i = 0; i < attribs.length; i++) {
		VertexAttrib& attrib = attribs[i];
		VkVertexInputAttributeDescription input_attribute_desc = {};
		input_attribute_desc.binding = binding;
		input_attribute_desc.location = vertex_inputs.length;
		input_attribute_desc.offset = attrib.offset;

		if (attrib.kind == VertexAttrib::Float) {
			VkFormat format[4] = { VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT };
			input_attribute_desc.format = format[attrib.length - 1];
		}
		if (attrib.kind == VertexAttrib::Int) {
			VkFormat format[4] = { VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32B32_SINT, VK_FORMAT_R32G32B32A32_SINT };
			input_attribute_desc.format = format[attrib.length - 1];
		}

		vertex_inputs.append(input_attribute_desc);
	}
}

ArrayVertexInputs input_attributes(BufferAllocator& self, VertexLayout layout) {
	return self.vertex_buffers[layout].input_desc.copy();
}

VkVertexInputBindingDescription input_bindings(BufferAllocator& self, VertexLayout layout) {
	return self.vertex_buffers[layout].binding_desc;
}

ArrayVertexInputs input_attributes(BufferAllocator& self, VertexLayout layout, InstanceLayout instance_layout) {
	return self.instance_buffers[instance_layout].input_desc.copy();
}

ArrayVertexBindings input_bindings(BufferAllocator& self, VertexLayout layout, InstanceLayout instance_layout) {
	return {
		self.vertex_buffers[layout].binding_desc,
		self.instance_buffers[layout].binding_desc
	};
}

//todo, instance layout description depends on VertexLayout but not the underlying buffers	
void alloc_layout_allocator(BufferAllocator& self, VertexLayoutDesc& vert_desc) {
	VkVertexInputBindingDescription vertex_binding_description = {};
	vertex_binding_description.binding = 0;
	vertex_binding_description.stride = vert_desc.elem_size;
	vertex_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VertexBufferAllocator& allocator = self.vertex_buffers[vert_desc.layout];
	allocator.layout = vert_desc.layout;
	allocator.binding_desc = vertex_binding_description;
	input_attributes(allocator.input_desc, vert_desc.attribs, 0);
	allocator.elem_size = vert_desc.elem_size;	
	allocator.vertex_capacity = vert_desc.vertices_size;
	allocator.index_capacity = vert_desc.indices_size;

	make_Buffer(self.device, self.physical_device, vert_desc.vertices_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocator.vbo, allocator.vbo_memory);
	make_Buffer(self.device, self.physical_device, vert_desc.indices_size,  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocator.ebo, allocator.ebo_memory);
}

void alloc_layout_allocator(BufferAllocator& m, VertexLayoutDesc& vert_desc, InstanceLayoutDesc& instance_desc) {
	alloc_layout_allocator(m, vert_desc);

	VkVertexInputBindingDescription instance_binding_description = {};
	instance_binding_description.binding = 1;
	instance_binding_description.stride = instance_desc.elem_size;
	instance_binding_description.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

	InstanceBufferAllocator& allocator = m.instance_buffers[instance_desc.layout];
	allocator.vertex_layout = vert_desc.layout;
	allocator.layout = instance_desc.layout;
	allocator.binding_desc = instance_binding_description;
	allocator.input_desc = m.vertex_buffers[vert_desc.layout].input_desc.copy();
	input_attributes(allocator.input_desc, instance_desc.attribs, 1);

	allocator.elem_size = instance_desc.elem_size;
	allocator.capacity = instance_desc.size;

	make_Buffer(m.device, m.physical_device, instance_desc.size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocator.buffer, allocator.buffer_memory);
}

VertexBuffer alloc_vertex_buffer(BufferAllocator& self, VertexLayout layout, int vertices_length, void* vertices, int indices_length, uint* indices) {		
	VertexBufferAllocator& allocator = self.vertex_buffers[layout];
	int vert_size = allocator.elem_size;
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
	int index_offset = allocator.index_offset;
	
	buffer.vertex_base = vertex_offset / vert_size;
	buffer.index_base = index_offset / index_size;	
	
	allocator.vertex_offset += vertices_size;
	allocator.index_offset += indices_size;

	printf("OFFSET vertex : %i, index : %i\n", vertex_offset, index_offset);

	if (vertices) staged_copy(self.device, self.staging, allocator.vbo, vertex_offset, vertices, vertices_size);
	if (indices) staged_copy(self.device, self.staging, allocator.ebo, index_offset, indices, indices_size);


	
	return buffer;
}

void begin_buffer_upload(BufferAllocator& self) {
	begin_staging_cmds(self.staging.queue);
}

void end_buffer_upload(BufferAllocator& self) {
	end_staging_cmds(self.staging.queue);
	self.staging.offset = 0;
}

void upload_data(BufferAllocator& self, InstanceBuffer& buffer, int length, void* data) {
	InstanceBufferAllocator& allocator = self.instance_buffers[buffer.layout];
	i64 size = allocator.elem_size * length;
	i64 offset = allocator.elem_size * buffer.base;
		
	assert(length <= buffer.capacity);		
	buffer.length = length;

	staged_copy(self.device, self.staging, allocator.buffer, offset, data, size);
}

InstanceBuffer alloc_instance_buffer(BufferAllocator& self, VertexLayout v_layout, InstanceLayout layout, int length, void* data) {
	InstanceBufferAllocator& allocator = self.instance_buffers[layout];
	int elem = allocator.elem_size;
	int size = length * elem;

	assert(allocator.capacity >= allocator.offset + size);

	InstanceBuffer buffer;
	buffer.vertex_layout = v_layout;
	buffer.layout = layout;
	buffer.base = allocator.offset / elem;
	buffer.capacity = length;
	buffer.length = length;

	allocator.offset += size;

	if (data) upload_data(self, buffer, length, data);

	return buffer;
}

void bind_vertex_buffer(BufferAllocator& self, VkCommandBuffer cmd_buffer, VertexLayout v_layout, InstanceLayout layout) {
	VertexBufferAllocator& vertex_buffer = self.vertex_buffers[v_layout];
	InstanceBufferAllocator& instance_buffer = self.instance_buffers[layout];

	VkDeviceSize offset = 0;

	//todo can we merge this!
	vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &vertex_buffer.vbo, &offset);
	vkCmdBindVertexBuffers(cmd_buffer, 1, 1, &instance_buffer.buffer, &offset);
	vkCmdBindIndexBuffer(cmd_buffer, vertex_buffer.ebo, 0, VK_INDEX_TYPE_UINT32);
}

void render_vertex_buffer(BufferAllocator& self, VertexBuffer& buffer) {
	/**/
}

BufferAllocator* make_BufferAllocator(RHI& rhi) {
	BufferAllocator* self = PERMANENT_ALLOC(BufferAllocator);

	self->device = get_Device(rhi);
	self->physical_device = get_PhysicalDevice(rhi);
	VkCommandPool cmd_pool = get_CommandPool(rhi);
	VkQueue graphics_queue = get_GraphicsQueue(rhi);

	self->staging.queue = make_StagingQueue(self->device, self->physical_device, cmd_pool, graphics_queue);
	self->staging.capacity = mb(50);
	make_Buffer(self->device, self->physical_device, self->staging.capacity, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
		self->staging.buffer, self->staging.buffer_memory);

	vector<VertexAttrib> default_model_layout = {
		{ 3, VertexAttrib::Float, offsetof(Vertex, position) },
		{ 3, VertexAttrib::Float, offsetof(Vertex, normal) },
		{ 2, VertexAttrib::Float, offsetof(Vertex, tex_coord) },
		{ 3, VertexAttrib::Float, offsetof(Vertex, tangent) },
		{ 3, VertexAttrib::Float, offsetof(Vertex, bitangent) }
	};

	VertexLayoutDesc vertex_layout_desc;
	vertex_layout_desc.layout = VERTEX_LAYOUT_DEFAULT;
	vertex_layout_desc.elem_size = sizeof(Vertex);
	vertex_layout_desc.vertices_size = mb(50);
	vertex_layout_desc.indices_size = mb(10);
	vertex_layout_desc.attribs = {
		{ 3, VertexAttrib::Float, offsetof(Vertex, position) },
		{ 3, VertexAttrib::Float, offsetof(Vertex, normal) },
		{ 2, VertexAttrib::Float, offsetof(Vertex, tex_coord) },
		{ 3, VertexAttrib::Float, offsetof(Vertex, tangent) },
		{ 3, VertexAttrib::Float, offsetof(Vertex, bitangent) }
	};

	InstanceLayoutDesc layout_desc_mat4x4;
	layout_desc_mat4x4.layout = INSTANCE_LAYOUT_MAT4X4;
	layout_desc_mat4x4.elem_size = sizeof(glm::mat4);
	layout_desc_mat4x4.size = mb(5);
	layout_desc_mat4x4.attribs = {
		{ 4, VertexAttrib::Float, sizeof(float) * 0 },
		{ 4, VertexAttrib::Float, sizeof(float) * 4 },
		{ 4, VertexAttrib::Float, sizeof(float) * 8 },
		{ 4, VertexAttrib::Float, sizeof(float) * 12 }
	};

	InstanceLayoutDesc layout_desc_terrain_chunk;
	layout_desc_terrain_chunk.layout = INSTANCE_LAYOUT_TERRAIN_CHUNK;
	layout_desc_terrain_chunk.elem_size = sizeof(ChunkInfo);
	layout_desc_terrain_chunk.size = mb(5);
	layout_desc_terrain_chunk.attribs = layout_desc_mat4x4.attribs;
	layout_desc_terrain_chunk.attribs.append({ 2, VertexAttrib::Float, offsetof(ChunkInfo, displacement_offset) });

	alloc_layout_allocator(*self, vertex_layout_desc, layout_desc_mat4x4);
	alloc_layout_allocator(*self, vertex_layout_desc, layout_desc_terrain_chunk);

	return self;
}

//todo probably better to pass a staging queue in the constructor
StagingQueue& get_StagingQueue(BufferAllocator& self) {
	return self.staging.queue;
}

void destroy_BufferAllocator(BufferAllocator* self) {
	VkDevice device = self->device;

	vkDestroyBuffer(device, self->staging.buffer, nullptr);
	vkFreeMemory(device, self->staging.buffer_memory, nullptr);

	for (int i = 0; i < VERTEX_LAYOUT_COUNT; i++) {
		VertexBufferAllocator& buffer = self->vertex_buffers[i];
		if (!buffer.vbo) continue;
		
		vkDestroyBuffer(device, buffer.vbo, nullptr);
		vkFreeMemory(device, buffer.vbo_memory, nullptr);

		vkDestroyBuffer(device, buffer.ebo, nullptr);
		vkFreeMemory(device, buffer.ebo_memory, nullptr);
	}

	for (int i = 0; i < INSTANCE_LAYOUT_COUNT; i++) {
		InstanceBufferAllocator& buffer = self->instance_buffers[i];
		if (!buffer.buffer) continue;

		vkDestroyBuffer(device, buffer.buffer, nullptr);
		vkFreeMemory(device, buffer.buffer_memory, nullptr);
	}
}

#endif