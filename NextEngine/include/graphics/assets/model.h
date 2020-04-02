#pragma once

#include "core/reflection.h"
#include "core/container/sstring.h"
#include "core/container/slice.h"
#include "core/container/handle_manager.h"
#include "core/handle.h"
#include "graphics/rhi/buffer.h"
#include "graphics/culling/aabb.h"
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

struct ENGINE_API Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 tex_coord;
	glm::vec3 tangent;
	glm::vec3 bitangent;

	REFLECT()
};

struct Mesh {
	VertexBuffer buffer;
	slice<Vertex> vertices;
	slice<uint> indices;
	AABB aabb;
	uint material_id;

	REFLECT()
};

struct Model {
	sstring path;
	slice<Mesh> meshes;
	slice<sstring> materials;
	AABB aabb;

	REFLECT()
};

struct ModelRenderer {
	bool visible = true;
	model_handle model_id;

	REFLECT()
};

struct Level;

 struct ENGINE_API ModelManager : HandleManager<Model, model_handle> {
	Level& level;

	ModelManager(Level& level);
	model_handle load(string_view, bool serialized = false);
	void load_in_place(model_handle, const glm::mat4& mat = glm::mat4(1.0));
};