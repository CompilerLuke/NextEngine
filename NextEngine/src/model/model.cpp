#include "stdafx.h"
#include "model/model.h"

REFLECT_STRUCT_BEGIN(Model)
REFLECT_STRUCT_MEMBER(path)
REFLECT_STRUCT_MEMBER(meshes)
REFLECT_STRUCT_MEMBER(materials)
REFLECT_STRUCT_END()

REFLECT_GENERIC_STRUCT_BEGIN(Handle<Model>)
REFLECT_STRUCT_MEMBER(id)
REFLECT_STRUCT_END()

void Model::on_load() {
	for (auto& mesh : meshes) {
		mesh.submit();
	}
}

void Model::render(ID id, glm::mat4* model, vector<Material>& materials, RenderParams& params) {
	for (auto& mesh : meshes) {
		mesh.render(id, model, materials, params);
	}
}

