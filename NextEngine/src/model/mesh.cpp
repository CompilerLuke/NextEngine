#include "stdafx.h"
#include "model/model.h"
#include "ecs/ecs.h"
#include "core/temporary.h"
#include "graphics/draw.h"
#include "graphics/rhi.h"

REFLECT_STRUCT_BEGIN(Vertex)
REFLECT_STRUCT_MEMBER(position)
REFLECT_STRUCT_MEMBER(normal)
REFLECT_STRUCT_MEMBER(tex_coord)
REFLECT_STRUCT_MEMBER(tangent)
REFLECT_STRUCT_MEMBER(bitangent)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Mesh)
REFLECT_STRUCT_MEMBER(material_id)
REFLECT_STRUCT_MEMBER(vertices)
REFLECT_STRUCT_MEMBER(indices)
REFLECT_STRUCT_END()

void Mesh::submit() {
	vector<VertexAttrib> attribs = {
		{3, Float, offsetof(Vertex, position)},
		{3, Float, offsetof(Vertex, normal)},
		{2, Float, offsetof(Vertex, tex_coord)},
		{3, Float, offsetof(Vertex, tangent)},
		{3, Float, offsetof(Vertex, bitangent)}
	};

	for (auto &v : vertices) {
		this->aabb.update(v.position);
	}

	new (&this->buffer) VertexBuffer(vertices, indices, attribs);
}

void Mesh::render(ID id, glm::mat4* model, vector<Handle<Material>>& materials, RenderParams& params) {
	auto material = RHI::material_manager.get(materials[material_id]);
	auto aabb = TEMPORARY_ALLOC(AABB);
	*aabb = this->aabb.apply(*model);

	DrawCommand cmd(id, model, aabb, &buffer, material);
	params.command_buffer->submit(cmd);
}
