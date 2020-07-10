#pragma once

#include <string>
#include "core/container/event_dispatcher.h"
#include "ecs/id.h"
#include "core/reflection.h"
#include "core/container/vector.h"
#include "core/container/string_buffer.h"

REFL
struct EntityNode {
	sstring name = "Root";
	ID id = 0;
	bool expanded = true;
	vector<EntityNode> children;
};

/*
COMP
struct EntityEditor { //Editor meta data for this entity
	string_buffer name;
	vector<ID> children;
};*/

struct Lister {
	EntityNode root_node;
	EntityNode* by_id[MAX_ENTITIES];

	string_buffer filter;

	void render(struct World& world, struct Editor& editor, struct RenderPass& params);
};

void register_entity(Lister&, string_view, ID);
string_buffer name_with_id(struct World&, ID id);