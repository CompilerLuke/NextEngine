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

namespace RHI {
	VertexBuffer alloc_vertex_buffer(VertexLayout layout, int vertices_length, void* vertices, int indices_length, uint* indices);
	InstanceBuffer alloc_instance_buffer(VertexLayout v_layout, InstanceLayout layout, int length, void* data);

	template<typename T>
	VertexBuffer alloc_vertex_buffer(VertexLayout layout, vector<T>& vertices, vector<unsigned int>& indices) {
		return alloc_vertex_buffer(layout, vertices.length, vertices.data, indices.length, indices.data);
	}

	template<typename T>
	InstanceBuffer alloc_instance_buffer(VertexLayout v_layout, InstanceLayout layout, vector<T>& data) {
		return alloc_instance_buffer(v_layout, layout, data.length, data.data);
	}

	void upload_data(InstanceBuffer& buffer, int length, void* data);

	template<typename T>
	void upload_data(InstanceBuffer& buffer, vector<T>& data) {
		upload_data(buffer, data.length, data.data);
	}

	void bind_vertex_buffer(VertexLayout v_layout, InstanceLayout layout = INSTANCE_LAYOUT_MAT4X4);
	void render_vertex_buffer(VertexBuffer& vertex_buffer);

	void create_buffers();
	void destroy_buffers();
}