#pragma once

#include <string>
#include "core/container/event_dispatcher.h"
#include "ecs/id.h"
#include "core/reflection.h"
#include "core/container/vector.h"
#include "core/container/string_buffer.h"

struct EntityEditor { //Editor meta data for this entity
	string_buffer name;
	vector<ID> children;

	REFLECT(NO_ARG)
};

struct Lister {
	string_buffer filter;

	void render(struct World& world, struct Editor& editor, struct RenderCtx& params);
};

string_buffer name_with_id(struct World&, ID id);