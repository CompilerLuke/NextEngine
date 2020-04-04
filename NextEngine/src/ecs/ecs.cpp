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

	while (true) {
		ID new_id = current_id++;
		bool skip_this_id = false;
		for (ID skip : skipped_ids) {
			if (new_id == skip) skip_this_id = true;
		}
		if (!skip_this_id) return new_id;
	}
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

void World::operator=(const World& other) {
	for (int i = 0; i < World::components_hash_size; i++) {
		ComponentStore* other_comp_store = other.components[i].get();
		ComponentStore* comp_store = components[i].get();

		if (other_comp_store == NULL) continue;

		if (comp_store) {
			*comp_store = *other_comp_store;
		}
		else {
			comp_store = other_comp_store->clone();
			components[i] = std::unique_ptr<ComponentStore>(comp_store);
		}

		comp_store->copy_from(other_comp_store);
	}

	skipped_ids = other.skipped_ids;
	delay_free_entity = other.delay_free_entity;
}

#endif