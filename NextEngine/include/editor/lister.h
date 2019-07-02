#pragma once

#include <string>
#include "core/eventDispatcher.h"
#include "ecs/id.h"
#include "reflection/reflection.h"
#include "core/vector.h"

struct EntityEditor { //Editor meta data for this entity
	std::string name;
	vector<ID> children;

	REFLECT()
};

struct Lister {
	std::string filter;

	void render(struct World& world, struct Editor& editor, struct RenderParams& params);
};

std::string name_with_id(struct World&, ID id);