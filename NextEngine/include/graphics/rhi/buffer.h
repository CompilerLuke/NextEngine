#pragma once

#include "engine/core.h"
#include "core/container/slice.h"

enum VertexLayout {
	VERTEX_LAYOUT_DEFAULT,
	VERTEX_LAYOUT_SKINNED,
	VERTEX_LAYOUT_COUNT
};

enum InstanceLayout {
	INSTANCE_LAYOUT_NONE,
	INSTANCE_LAYOUT_MAT4X4,
	INSTANCE_LAYOUT_TERRAIN_CHUNK,
	INSTANCE_LAYOUT_COUNT
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

struct VertexStreaming;
struct RHI;


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

