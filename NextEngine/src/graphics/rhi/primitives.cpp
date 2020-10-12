#include "graphics/rhi/primitives.h"
#include "graphics/rhi/buffer.h"
#include "graphics/assets/model.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/assets_store.h"

Primitives primitives;

void init_primitives() {
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

	Model& model = *PERMANENT_ALLOC(Model);
	model.materials.data = PERMANENT_ALLOC(sstring);
	model.materials.length = 1;
	model.materials[0] = "default_material";

	model.aabb.max = glm::vec3(0.5, 0, 0.5);
	model.aabb.min = glm::vec3(-0.5, 0, -0.5);

	model.meshes.data = PERMANENT_ALLOC(Mesh);
	model.meshes.length = 1;
	model.meshes[0].buffer[0] = alloc_vertex_buffer(VERTEX_LAYOUT_DEFAULT, 4, vertices, 6, indices);

	primitives.quad = assets.models.assign_handle(std::move(model), true);
	primitives.cube = load_Model("engine/cube.fbx");
	primitives.sphere = load_Model("engine/sphere.fbx");

	primitives.quad_buffer   = get_Model(primitives.quad)->meshes[0].buffer[0];
	primitives.cube_buffer   = get_Model(primitives.cube)->meshes[0].buffer[0];
	primitives.sphere_buffer = get_Model(primitives.sphere)->meshes[0].buffer[0];
	//first_quad = false;
}

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

		quad_mesh = alloc_vertex_buffer(VERTEX_LAYOUT_DEFAULT, 4, vertices, 6, indices);
		first_quad = false;
	}

	//bind_vertex_buffer(buffer_manager, VERTEX_LAYOUT_DEFAULT);
	//render_vertex_buffer(buffer_manager, quad_mesh);
}
