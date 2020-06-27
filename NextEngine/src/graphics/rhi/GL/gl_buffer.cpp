
#ifdef RENDER_API_OPENGL

//LAYOUTS
#include "graphics/assets/model.h"
#include "graphics/renderer/terrain.h"
#include <glad/glad.h>
#include "graphics/rhi/buffer.h"
#include "core/io/logger.h"

namespace RHI {
	struct VertexAttrib {
		enum Kind {
			Float, Int
		};

		unsigned int length;
		Kind kind;
		unsigned int offset;
	};

	GLenum gl_attrib_kind[2] = { GL_FLOAT, GL_INT };

	struct VertexBufferAllocator {
		int vbo = 0;
		int ebo = 0;
		int vertex_capacity = 0;
		int index_capacity = 0;
		int vertex_length = 0;
		int index_offset = 0;
		int elem_size;
	};

	struct InstanceBufferAllocator {
		int vao = 0;
		int buffer = 0;
		int capacity = 0;
		int offset = 0;
		int elem_size;
	};

	VertexBufferAllocator vertex_buffers[VERTEX_LAYOUT_COUNT];
	InstanceBufferAllocator instance_buffers[VERTEX_LAYOUT_COUNT][INSTANCE_LAYOUT_COUNT];

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

	struct LayoutDesc {
		VertexLayoutDesc vertex;
		vector<InstanceLayoutDesc> instances;
	};

	void alloc_layout_allocator(LayoutDesc& desc) {
		VertexLayoutDesc& vert_desc = desc.vertex;
		VertexBufferAllocator& buffer = vertex_buffers[desc.vertex.layout];

		uint vbo = 0;
		uint ebo = 0;

		glGenBuffers(1, &vbo);
		glGenBuffers(1, &ebo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vert_desc.vertices_size, NULL, GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, vert_desc.indices_size, NULL, GL_STATIC_DRAW);

		for (InstanceLayoutDesc& instance_desc : desc.instances) {
			InstanceBufferAllocator& buffer = instance_buffers[vert_desc.layout][instance_desc.layout];
			
			GLuint vao, obj = 0;
			glGenVertexArrays(1, &vao);
			glGenBuffers(1, &obj);

			glBindVertexArray(vao);

			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	
			for (int i = 0; i < vert_desc.attribs.length; i++) {
				VertexAttrib& va = vert_desc.attribs[i];

				glEnableVertexAttribArray(i);
				glVertexAttribPointer(i, va.length, gl_attrib_kind[va.kind], false, vert_desc.elem_size, (void*)((std::size_t)va.offset));
			}

			glBindBuffer(GL_ARRAY_BUFFER, obj);
			glBufferData(GL_ARRAY_BUFFER, instance_desc.size, NULL, GL_STREAM_DRAW);

			uint offset = vert_desc.attribs.length;

			for (int i = 0; i < instance_desc.attribs.length; i++) {
				VertexAttrib& attrib = instance_desc.attribs[i];

				glEnableVertexAttribArray(offset + i);
				glVertexAttribPointer(offset + i, 4, gl_attrib_kind[attrib.kind], false, instance_desc.elem_size, (void*)(attrib.offset));
				glVertexAttribDivisor(offset + i, 1);
			}

			glBindVertexArray(0);

			assert(buffer.vao == 0);
			buffer.vao = vao;			
			buffer.buffer = obj;
			buffer.capacity = instance_desc.size;
			buffer.elem_size = instance_desc.elem_size;
		}

		assert(buffer.vbo == 0);

		buffer.vbo = vbo;
		buffer.ebo = ebo;
		buffer.vertex_capacity = vert_desc.vertices_size;
		buffer.index_capacity = vert_desc.indices_size;
		buffer.elem_size = vert_desc.elem_size;
	}

	VertexBuffer alloc_vertex_buffer(VertexLayout layout, int vertices_length, void* vertices, int indices_length, uint* indices) {		
		VertexBufferAllocator* allocator = &vertex_buffers[layout];
		int elem_size = allocator->elem_size;
		int vertices_size = vertices_length * elem_size;
		int indices_size = indices_length * sizeof(uint);



		log("UPLOADING VERTEX BUFFER Vertex:", elem_size, " ", vertices_length, ", size ",  vertices_size, " ", indices_size, "\n");
		

		assert(allocator->vertex_capacity >= allocator->vertex_length * elem_size + vertices_size);
		assert(allocator->index_capacity >= allocator->index_offset + indices_length * sizeof(uint));
		
		VertexBuffer buffer;
		buffer.layout = layout;
		buffer.length = indices_length;

		buffer.vertex_offset = allocator->vertex_length * elem_size;
		buffer.index_offset = allocator->index_offset;

		for (int i = 0; i < indices_length; i++) {
			indices[i] += allocator->vertex_length;
		}

		allocator->vertex_length += vertices_length;
		allocator->index_offset += indices_size;

		glBindBuffer(GL_ARRAY_BUFFER, allocator->vbo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, allocator->ebo);

		glBufferSubData(GL_ARRAY_BUFFER, buffer.vertex_offset, vertices_size, vertices);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, buffer.index_offset, indices_size, indices);

		return buffer;
	}

	void upload_data(InstanceBuffer& buffer, int length, void* data) {
		InstanceBufferAllocator allocator = instance_buffers[buffer.vertex_layout][buffer.layout];
		int size = allocator.elem_size * length;
		
		assert(size <= buffer.capacity);		
		buffer.size = size;

		GLuint underlying_buffer = allocator.buffer;

		glBindBuffer(GL_ARRAY_BUFFER, underlying_buffer);
		glBufferSubData(GL_ARRAY_BUFFER, buffer.base * allocator.elem_size, size, data);
	}

	InstanceBuffer alloc_instance_buffer(VertexLayout v_layout, InstanceLayout layout, int length, void* data) {
		InstanceBufferAllocator* allocator = &instance_buffers[v_layout][layout];
		int elem = allocator->elem_size;
		int size = length * elem;

		assert(allocator->capacity >= allocator->offset + size);

		InstanceBuffer buffer;
		buffer.vertex_layout = v_layout;
		buffer.layout = layout;
		buffer.base = allocator->offset / elem;
		buffer.capacity = size;

		allocator->offset += size;

		if (data) upload_data(buffer, size, data);

		return buffer;
	}

	void bind_vertex_buffer(VertexLayout v_layout, InstanceLayout layout) {
		GLuint vao = instance_buffers[v_layout][layout].vao;
		glBindVertexArray(vao);
	}

	void render_vertex_buffer(VertexBuffer& buffer) {
		glDrawElements(GL_TRIANGLES, buffer.length, GL_UNSIGNED_INT, (void*)buffer.index_offset);
	}

	void create_buffers() {
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

		LayoutDesc layout_desc;
		layout_desc.vertex = vertex_layout_desc;
		layout_desc.instances = {layout_desc_mat4x4, layout_desc_terrain_chunk};

		alloc_layout_allocator(layout_desc);
	}

	void destroy_buffers() {
		GLuint vaos[VERTEX_LAYOUT_COUNT];
		GLuint array_buffers[VERTEX_LAYOUT_COUNT * INSTANCE_LAYOUT_COUNT];

		int vao_count = 0;
		int array_buffer_count = 0;

		for (int i = 0; i < VERTEX_LAYOUT_COUNT; i++) {
			for (int j = 0; j < INSTANCE_LAYOUT_COUNT; j++) {
				if (instance_buffers[i][j].vao == 0) continue;

				array_buffers[array_buffer_count++] = instance_buffers[i][j].buffer;
				vaos[vao_count++] = instance_buffers[i][j].vao;
			}
		}

		glDeleteVertexArrays(VERTEX_LAYOUT_COUNT, vaos);
		glDeleteBuffers(GL_ARRAY_BUFFER, array_buffers);
	}
}

#endif