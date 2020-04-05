#include "stdafx.h"
#include "graphics/rhi/primitives.h"
#include <glad/glad.h>
#include "graphics/rhi/buffer.h"
#include "graphics/assets/model.h"

bool first_quad = true;
VertexBuffer quad_mesh;



void render_quad() {
	if (first_quad) {
		glm::vec3 vertices_pos[4] = {
			glm::vec3(1,  1, 0),  // top right
			glm::vec3(1, -1, 0),  // bottom right
			glm::vec3(-1, -1, 0),  // bottom left
			glm::vec3(-1,  1, 0)   // top left 
		};

		glm::vec2 tex_coords[4] = {
			glm::vec2(1, 1),   // top right
			glm::vec2(1, 0.0),   // bottom right
			glm::vec2(0, 0),   // bottom left
			glm::vec2(0, 1)    // top left 
		};

		unsigned int indices[6] = {  // note that we start from 0!
			0, 1, 3,  // first Triangle
			1, 2, 3   // second Triangle
		};

		Vertex vertices[4] = {};

		for (int i = 0; i < 4; i++) {
			vertices[i].position = vertices_pos[i];
			vertices[i].tex_coord = tex_coords[i];
		}

		quad_mesh = RHI::alloc_vertex_buffer(VERTEX_LAYOUT_DEFAULT, 4, vertices, 6, indices);
		first_quad = false;
	}

	RHI::bind_vertex_buffer(VERTEX_LAYOUT_DEFAULT);
	RHI::render_vertex_buffer(quad_mesh);
}
