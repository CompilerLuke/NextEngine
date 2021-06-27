#include "mesh/input_mesh.h"
#include "mesh/input_mesh_bvh.h"

InputMeshRegistry::InputMeshRegistry(CFDDebugRenderer& debug) : debug(debug) {
	InputModel empty;
	empty.name = "Mesh Not Found";
	
	models.append(std::move(empty)); 
	bvh.append(InputModelBVH(models[0].surface[0], debug)); //todo: this will cause memory corruption on resize
}

InputMeshRegistry::~InputMeshRegistry() {

}

input_model_handle InputMeshRegistry::register_model(InputModel&& model) {
	models.append(std::move(model));
	bvh.append(InputModelBVH(models[models.length - 1].surface[0], debug)); //todo: this will cause memory corruption on resize
	//bvh[models.length - 1].build();
	return { models.length-1 };
}

InputModel& InputMeshRegistry::get_model(input_model_handle mesh) {
	if (mesh.id >= models.length) return models[0]; //return sentinel
	return models[mesh.id];
}


InputModelBVH& InputMeshRegistry::get_model_bvh(input_model_handle mesh) {
	if (mesh.id >= bvh.length) return bvh[0]; //return sentinel
	return bvh[mesh.id];
}