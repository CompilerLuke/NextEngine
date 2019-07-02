#include "stdafx.h"
#include "graphics/primitives.h"
#include <glad/glad.h>
#include "graphics/buffer.h"
#include "model/model.h"

bool first_quad = true;
Mesh quad_mesh;

void render_quad() {
	if (first_quad) {
		glm::vec3 vertices[4] = {
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

		for (int i = 0; i < 4; i++) {
			Vertex v;
			v.position = vertices[i];
			v.tex_coord = tex_coords[i];
			quad_mesh.vertices.append(v);
		}

		for (int i = 0; i < 6; i++) {
			quad_mesh.indices.append(indices[i]);
		}

		quad_mesh.submit();
		first_quad = false;
	}

	quad_mesh.buffer.bind();
	glDrawElements(GL_TRIANGLES, quad_mesh.buffer.length, GL_UNSIGNED_INT, NULL);
}