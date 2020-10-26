#pragma once

#include "engine/handle.h"
#include "transform.h"
#include "core/container/vector.h"

COMP
struct Grass {
	model_handle placement_model = { INVALID_HANDLE };
	bool cast_shadows = false;

	float width = 5.0f;
	float height = 5.0f;
	float max_height = 20.0f;
	float density = 0.5f;

	float random_rotation = 1.0f;
	float random_scale = 1.0f;
	bool align_to_terrain_normal = false;


	vector<glm::vec3 > positions;
	vector<glm::mat4> model_m;
};