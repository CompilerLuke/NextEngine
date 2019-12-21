#pragma once

#include "core/string_buffer.h"
#include "core/vector.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "ecs/ecs.h"
#include "reflection/reflection.h"
#include "core/handle.h"
#include "graphics/buffer.h"

struct Material;

struct ENGINE_API Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 tex_coord;
	glm::vec3 tangent;
	glm::vec3 bitangent;

	REFLECT(NO_ARG)
};

struct ENGINE_API Mesh {
	VertexBuffer buffer;
	vector<Vertex> vertices;
	vector<unsigned int> indices;
	AABB aabb;
	unsigned int material_id;

	Mesh() {};
	Mesh(const Mesh&);

	void submit();
	void render(ID, glm::mat4*, vector<Handle<Material>>&, RenderParams&);

	REFLECT(NO_ARG)
};

struct ENGINE_API Model {
	StringBuffer path;
	vector<Mesh> meshes;
	vector<StringBuffer> materials;

	AABB aabb;

	void on_load();
	void load_in_place(const glm::mat4& apply_transform = glm::mat4(1.0));
	void render(ID, glm::mat4*, vector<Handle<Material>>&, RenderParams&);

	REFLECT(NO_ARG)
};

Handle<Model> ENGINE_API load_Model(StringView, bool serialized = false);

struct ModelRenderer {
	bool visible = true;
	Handle<Model> model_id;

	REFLECT()
};
