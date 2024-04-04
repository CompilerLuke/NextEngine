#pragma once

#include <glm/mat4x4.hpp>
#include "ecs/id.h"

void ENGINE_API compute_model_matrices(glm::mat4* model_m, struct World& world, EntityQuery mask);