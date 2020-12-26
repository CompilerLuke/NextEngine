#pragma once

#include "engine/core.h"
#include "core/container/slice.h"
#include "core/container/array.h"
#include <memory.h>

enum VertexLayout {
	VERTEX_LAYOUT_DEFAULT,
	VERTEX_LAYOUT_SKINNED,
	VERTEX_LAYOUT_COUNT,
    VERTEX_LAYOUT_MAX = 10,
};

enum InstanceLayout {
	INSTANCE_LAYOUT_NONE,
	INSTANCE_LAYOUT_MAT4X4,
	INSTANCE_LAYOUT_TERRAIN_CHUNK,
	INSTANCE_LAYOUT_COUNT,
    INSTANCE_LAYOUT_MAX = 10,
};

struct VertexAttrib {
    enum Type {
        Float, Int, Unorm
    };

    uint length;
    Type type;
    uint offset;
};

struct VertexLayoutDesc {
    array<20, VertexAttrib> attribs;
    uint elem_size;
};

struct InstanceLayoutDesc {
    array<20, VertexAttrib> attribs;
    uint elem_size;
};

struct Arena {
    int capacity;
    int base_offset;
    int offset;
};


//Length and capacity are the number of elements, not bytes
struct ENGINE_API VertexBuffer {
	VertexLayout layout;
	int vertex_base = 0;
	int index_base = 0;
	int length = 0;
	int vertex_capacity = 0;
	int instance_capacity = 0;
};

struct ENGINE_API InstanceBuffer {
	InstanceLayout layout;
	int base = 0;
	int length = 0;
	int capacity = 0;
};

enum UBOUpdateFlags {
	UBO_PERMANENT_MAP,
	UBO_MAPPED_INIT,
	UBO_MAP_UNMAP,
	UBO_UPDATE_MODE_COUNT
};

struct ENGINE_API UBOBuffer {
	UBOUpdateFlags flags;
	int offset;
	int size;
};

enum DeviceMemoryFlags {
    MEMORY_GPU_NATIVE,
    MEMORY_CPU_WRITEABLE,
    MEMORY_CPU_READABLE,
};

enum BufferUsageFlags {
    BUFFER_VERTEX,
    BUFFER_INDEX,
    BUFFER_UBO,
};

struct buffer_handle {
    uint id = 0;
};

struct device_memory_handle {
    uint id = 0;
};

struct VertexStreaming;
struct RHI;

struct MemoryRequirements {
    u64 size;
    uint alignment;
    uint resource_type;
};

struct CPUVisibleBuffer {
    buffer_handle buffer;
    device_memory_handle memory;
    u64 capacity;
    void* mapped;
};

ENGINE_API device_memory_handle alloc_device_memory(uint size, uint resource_type, DeviceMemoryFlags);
ENGINE_API void dealloc_device_memory(device_memory_handle);

ENGINE_API void* map_memory(device_memory_handle, u64 offset, u64 size);
ENGINE_API void unmap_memory(device_memory_handle);

ENGINE_API MemoryRequirements query_buffer_memory_requirements(buffer_handle);

ENGINE_API buffer_handle alloc_buffer(u64 size, BufferUsageFlags usage, device_memory_handle memory, u64 offset);
ENGINE_API buffer_handle alloc_buffer_and_memory(u64 size, BufferUsageFlags usage, device_memory_handle* memory, DeviceMemoryFlags memory_flags);
ENGINE_API void dealloc_buffer(buffer_handle buffer);

ENGINE_API CPUVisibleBuffer alloc_cpu_visibile_buffer(u64 size, BufferUsageFlags usage);
ENGINE_API void dealloc_cpu_visibile_buffer(CPUVisibleBuffer&);

ENGINE_API VertexLayout register_vertex_layout(VertexLayoutDesc&);

ENGINE_API VertexBuffer alloc_vertex_buffer(VertexLayout layout, int vertices_length, void* vertices, int indices_length, uint* indices);
ENGINE_API InstanceBuffer frame_alloc_instance_buffer(InstanceLayout layout, uint length, void** data);
//UBOBuffer frame_alloc_ubo_buffer(int size);
ENGINE_API UBOBuffer alloc_ubo_buffer(uint size, UBOUpdateFlags);

ENGINE_API void memcpy_ubo_buffer(UBOBuffer&, uint size, void* ptr);

template<typename T>
void memcpy_ubo_buffer(UBOBuffer& buffer, T* ptr) { memcpy_ubo_buffer(buffer, sizeof(T), (void*)ptr); }

template<typename T>
VertexBuffer alloc_vertex_buffer(VertexLayout layout, slice<T> vertices, slice<uint> indices) {
	return alloc_vertex_buffer(layout, vertices.length, vertices.data, indices.length, indices.data);
}



template<typename T>
InstanceBuffer frame_alloc_instance_buffer(InstanceLayout layout, const slice<T> data) {
	void* mapped;
	InstanceBuffer buffer = frame_alloc_instance_buffer(layout, data.length, &mapped);
	memcpy(mapped, data.data, sizeof(T) * data.length);
	return buffer;
}

