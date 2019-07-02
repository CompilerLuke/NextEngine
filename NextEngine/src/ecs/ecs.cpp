#include "stdafx.h"
#include "ecs/ecs.h"

#if 1

//REFLECT_ALIAS(ID, unsigned int) requires strong typedef
//REFLECT_ALIAS(Layermask, unsigned int) requires strong typedef

REFLECT_STRUCT_BEGIN(Entity)
REFLECT_STRUCT_MEMBER(enabled)
REFLECT_STRUCT_MEMBER_TAG(layermask, reflect::LayermaskTag)
REFLECT_STRUCT_END()

int global_type_id = 0;

ID World::make_ID() {
	if (freed_entities.length > 0) {
		return freed_entities.pop();
	}
	return current_id++;
}

void World::free_ID(ID id) {
	freed_entities.append(id);
}

vector<Component> World::components_by_id(ID id) {
	vector<Component> components;
	components.allocator = &temporary_allocator;

	for (int i = 0; i < components_hash_size; i++) {
		if (this->components[i]) {
			auto comp = this->components[i]->get_by_id(id);
			if (comp.data) components.append(comp);
		}
	}

	return components;
}
#endif