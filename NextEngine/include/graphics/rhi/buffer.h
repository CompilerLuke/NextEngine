#pragma once

#include "core/core.h"

enum VertexLayout {
	VERTEX_LAYOUT_DEFAULT,
	VERTEX_LAYOUT_SKINNED,
	VERTEX_LAYOUT_COUNT
};

enum InstanceLayout {
	INSTANCE_LAYOUT_MAT4X4,
	INSTANCE_LAYOUT_TERRAIN_CHUNK,
	INSTANCE_LAYOUT_COUNT
};

struct ENGINE_API VertexBuffer {
	VertexLayout layout;
	int vertex_offset = 0;
	int index_offset = 0;
	int length = 0;
	int size = 0;
};

struct ENGINE_API InstanceBuffer {
	VertexLayout vertex_layout;
	InstanceLayout layout;
	int base = 0;
	int size = 0;
	int capacity = 0;
};

struct BufferManager;
struct RHI;

VertexBuffer alloc_vertex_buffer(BufferManager& self, VertexLayout layout, int vertices_length, void* vertices, int indices_length, uint* indices);
InstanceBuffer alloc_instance_buffer(BufferManager& self, VertexLayout v_layout, InstanceLayout layout, int length, void* data);
void upload_data(BufferManager& self, InstanceBuffer& buffer, int length, void* data);
void bind_vertex_buffer(BufferManager&, VertexLayout v_layout, InstanceLayout layout = INSTANCE_LAYOUT_MAT4X4);
void render_vertex_buffer(BufferManager&, VertexBuffer& vertex_buffer);

BufferManager* make_BufferManager(RHI& rhi);
void destroy_BufferManager(BufferManager*);

template<typename T>
VertexBuffer alloc_vertex_buffer(BufferManager& self, VertexLayout layout, slice<T> vertices, slice<uint> indices) {
	return alloc_vertex_buffer(self, layout, vertices.length, vertices.data, indices.length, indices.data);
}

template<typename T>
InstanceBuffer alloc_instance_buffer(BufferManager& self, VertexLayout v_layout, InstanceLayout layout, slice<T>& data) {
	return alloc_instance_buffer(v_layout, layout, data.length, data.data);
}

template<typename T>
void upload_data(BufferManager& self, InstanceBuffer& buffer, vector<T>& data) {
	upload_data(self, buffer, data.length, data.data);
}

