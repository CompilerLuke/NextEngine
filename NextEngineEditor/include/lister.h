#pragma once

#include <string>
#include "core/eventDispatcher.h"
#include "ecs/id.h"
#include "reflection/reflection.h"
#include "core/vector.h"
#include "core/string_buffer.h"

struct EntityEditor { //Editor meta data for this entity
	StringBuffer name;
	vector<ID> children;

	REFLECT(NO_ARG)
};

struct Lister {
	StringBuffer filter;

	void render(struct World& world, struct Editor& editor, struct RenderParams& params);
};

StringBuffer name_with_id(struct World&, ID id);