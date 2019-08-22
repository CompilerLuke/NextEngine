#pragma once

#include <string>
#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "graphics/buffer.h"
#include "graphics/culling.h"
#include "graphics/materialSystem.h"
#include "ecs/ecs.h"
#include "reflection/reflection.h"
#include "core/handle.h"

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 tex_coord;
	glm::vec3 tangent;
	glm::vec3 bitangent;

	REFLECT()
};

struct Mesh {
	VertexBuffer buffer;
	vector<Vertex> vertices;
	vector<unsigned int> indices;
	AABB aabb;
	unsigned int material_id;

	Mesh() {};
	void submit();
	void render(ID, glm::mat4*, vector<Handle<Material>>&, RenderParams&);

	REFLECT()
};

struct Model {
	StringBuffer path;
	vector<Mesh> meshes;
	vector<StringBuffer> materials;

	AABB aabb;

	void on_load();
	void load_in_place(const glm::mat4& apply_transform = glm::mat4(1.0));
	void ENGINE_API render(ID, glm::mat4*, vector<Handle<Material>>&, RenderParams&);

	REFLECT()
};

Handle<Model> ENGINE_API load_Model(StringView, bool serialized = false);

struct ModelRenderer {
	bool visible = true;
	Handle<Model> model_id;

	REFLECT()
};
