#include "diffUtil.h"
#include "core/memory/allocator.h"
#include "core/memory/linear_allocator.h"
#include "core/io/logger.h"
#include <glm/gtc/epsilon.hpp>
#include "editor.h"

void clear_stack(ActionStack& stack) {
	stack.head = 0;
	stack.tail = 0;
	stack.stack.clear();
}

void init_stack(ActionStack& stack, u64 size) {
	stack = {};
	stack.capacity = kb(10);
	stack.block = PERMANENT_ARRAY(char, stack.capacity);
}

void init_actions(EditorActions& actions) {
	init_stack(actions.undo, kb(10));
	init_stack(actions.redo, kb(10));
	init_stack(actions.frame_diffs, kb(5));
}

void destroy_action(ActionStack& stack, EditorActionHeader& header) {
	switch (header.type) {
	case EditorActionHeader::Create_Entity:
	case EditorActionHeader::Destroy_Entity:
	case EditorActionHeader::Create_Component:
	case EditorActionHeader::Destroy_Component:
		EntityCopy* copy = (EntityCopy*)(header.ptr);
		/* todo call destructors and free ids! */
	}
}

bool enough_space_in_stack(ActionStack& stack, uint size) {
	if (stack.head >= stack.tail) return stack.head + size < stack.capacity;
	else stack.head + size < stack.tail;
}

char* stack_push_action(ActionStack& stack, EditorActionHeader& header) {
	uint size = header.size;
	assert(size <= stack.capacity);

	if (stack.stack.length == MAX_SAVED_UNDOS || !enough_space_in_stack(stack, size)) {
		if (stack.head > stack.tail) stack.head = 0;

		uint i;
		for (i = 0; i < stack.stack.length; i++) {
			EditorActionHeader header = stack.stack[i];
			stack.tail = (header.ptr - stack.block) + header.size;

			if (enough_space_in_stack(stack, size)) break;
		}

		uint shift = i + 1;

		for (uint i = 0; i < shift; i++) {
			destroy_action(stack, stack.stack[i]);
		}

		stack.stack.shift(shift);

		printf("Current head %i\n", stack.head);
	}
	
	char* ptr = stack.block + stack.head;
	header.ptr = ptr; 
	stack.head += size;
	
	stack.stack.append(header);
	
	return ptr;
}

template<typename T>
T* push_ptr(char** block, uint count = 1) {
	char* ptr = *block;
	*block += sizeof(T) * count;
	return (T*)ptr;
}

template<typename T>
T* copy_ptr(char** block, T* from, uint count) {
	T* to = push_ptr<T>(block, count);
	memcpy_t(to, from, count);
	return to;
}

template<typename T>
slice<T> copy_slice(char** block, slice<T> slice) {
	return {copy_ptr(block, slice.data, slice.length), slice.length};
}

void begin_tdiff(DiffUtil& util, void* ptr, refl::Type* type) {
	util.real_ptr = ptr;
	util.copy_ptr = get_temporary_allocator().allocate(type->size);
	util.type = type;
	
	memcpy(util.copy_ptr, util.real_ptr, type->size);
}

void begin_diff(DiffUtil& util, void* ptr, void* copy, refl::Type* type) {
	util.real_ptr = ptr;
	util.copy_ptr = copy;
	util.type = type;

	memcpy(util.copy_ptr, util.real_ptr, type->size);
}

bool submit_diff(ActionStack& stack, DiffUtil& util,  string_view string) {
	assert(util.type->type == refl::Type::Struct || util.type->type == refl::Type::Union);
	assert(util.type->size < 1000);

	tvector<uint> modified_fields;
	bool is_modified = false;

	if (util.type->type == refl::Type::Struct) {
		refl::Struct* type = (refl::Struct*)util.type;

		for (uint i = 0; i < type->fields.length; i++) {
			auto& member = type->fields[i];
			auto original_value = (char*)util.copy_ptr + member.offset;
			auto new_value = (char*)util.real_ptr + member.offset;

			if (memcmp(new_value, original_value, member.type->size) != 0) {
				modified_fields.append(i);
				is_modified = true;
			}
		}
	}
	else if (util.type->type == refl::Type::Union) {
		is_modified = memcmp(util.copy_ptr, util.real_ptr, util.type->size) != 0;
	}

	if (!is_modified) return false;

	EditorActionHeader header = {};
	header.type = EditorActionHeader::Diff;
	header.name = string;
	header.size = sizeof(Diff) + util.type->size + sizeof(uint) * modified_fields.length;

	char* buffer = stack_push_action(stack, header);

	Diff* diff = push_ptr<Diff>(&buffer);
	diff->id = util.id;
	diff->copy_ptr = copy_ptr<char>(&buffer, (char*)util.copy_ptr, util.type->size);
	diff->real_ptr = util.real_ptr;
	diff->type = util.type;
	diff->fields_modified = copy_slice<uint>(&buffer, modified_fields);

	return true;
}

//this will only work with flat data structures!
bool end_diff(EditorActions& stack, DiffUtil& util, string_view string) {
	bool modified = submit_diff(stack.undo, util, string);
	if (modified) stack.frame_diffs.stack.append(stack.undo.stack.last());
	return modified;
}

void commit_diff(EditorActions& stack, DiffUtil& util) {
	submit_diff(stack.frame_diffs, util, "");
}

EntityCopy* save_entity_copy(World& world, ActionStack& stack, Archetype copy_archetype, Archetype mask, ID id, EditorActionHeader& header) {
	auto components = world.components_by_id(id, copy_archetype);

	uint size = sizeof(EntityCopy);
	
	for (ComponentPtr& component : components) {
		size += world.component_size[component.component_id];
		size += sizeof(EntityCopy::Component);
	}

	header.size = size;

	char* data = stack_push_action(stack, header);

	EntityCopy* entity_copy = push_ptr<EntityCopy>(&data);
	entity_copy->id = id;
	entity_copy->from = world.arch_of_id(id);
	entity_copy->to = (entity_copy->from | copy_archetype) & mask;
	entity_copy->components.data = push_ptr<EntityCopy::Component>(&data, components.length);
	entity_copy->components.length = components.length;

	printf("=============\n");

	for (uint i = 0; i < components.length; i++) {
		ComponentPtr& component_ptr = components[i];
		uint size = world.component_size[component_ptr.component_id];

		EntityCopy::Component& component = entity_copy->components[i];
		component.component_id = component_ptr.component_id;
		component.ptr = copy_ptr<char>(&data, (char*)component_ptr.data, size);
		//component.size = size;

		printf("Ptr of save entity copy %p\n", component.ptr);

		if (component.component_id == 0) {
			Entity* e = (Entity*)(component.ptr.data());
			printf("SAVED ENTITY id: %i\n", e->id);
		}
	}

	return entity_copy;
}

void* copy_onto_stack(ActionStack& stack, EditorActionHeader& header) {
	EditorActionHeader copy = header;
	char* buffer = stack_push_action(stack, copy);
	memcpy(buffer, header.ptr, header.size);
	return copy.ptr;
}

void apply_diff_and_move_to(ActionStack& actions, EditorActionHeader& header, Diff* diff) {
	Diff* copy = (Diff*)copy_onto_stack(actions, header);

	memcpy(copy->copy_ptr, diff->real_ptr, diff->type->size);
	memcpy(diff->real_ptr, diff->copy_ptr, diff->type->size);
}

void destroy_components(World& world, EntityCopy* copy) {
	world.change_archetype(copy->id, copy->from, copy->to, false);
	std::swap(copy->from, copy->to);
}

void create_components(World& world, EntityCopy* copy) {
	world.change_archetype(copy->id, copy->from, copy->to, false);
	std::swap(copy->from, copy->to);

	for (EntityCopy::Component& component : copy->components) {
		u8* ptr = (u8*)world.id_to_ptr[component.component_id][copy->id];
		memcpy(ptr, component.ptr.data(), world.component_size[component.component_id]);

		if (component.component_id == 0) {
			Entity* e = (Entity*)(component.ptr.data());
			printf("SAVED ENTITY id: %i\n", e->id);
		}
	}
}

//NOTE Somewhat counter intiutive behaviour, since calling destroy or create components will flip the from/to

void entity_create_action(EditorActions& actions, ID id) {
	char desc[50];
	sprintf_s(desc, "CREATED ENTITY #%i", id);

	EditorActionHeader header{ EditorActionHeader::Create_Entity , 0, 0, desc};
	save_entity_copy(actions.world, actions.undo, ANY_ARCHETYPE, 0, id, header);
	actions.frame_diffs.stack.append(header);
}

void entity_destroy_action(EditorActions& actions, ID id) {
	char desc[50];
	sprintf_s(desc, "DESTROYED ENTITY #%i", id);
	
	EditorActionHeader header{ EditorActionHeader::Destroy_Entity, 0, 0, desc };
	EntityCopy* copy = save_entity_copy(actions.world, actions.undo, ANY_ARCHETYPE, 0, id, header);
	destroy_components(actions.world, copy);

	actions.frame_diffs.stack.append(header);
}

void entity_create_component_action(EditorActions& actions, ComponentID component_id, ID id) {
	char desc[50];
	sprintf_s(desc, "CREATED COMPONENT %s #i", actions.world.component_type[component_id], id);

	Archetype component_mask = 1ull << component_id;
	Archetype arch = actions.world.arch_of_id(id);

	actions.world.change_archetype(id, arch, arch | component_mask, true);

	EditorActionHeader header{ EditorActionHeader::Create_Component, 0, 0, desc };
	EntityCopy* copy = save_entity_copy(actions.world, actions.undo, component_mask, ~component_mask, id, header);

	actions.frame_diffs.stack.append(header);
}

void entity_destroy_component_action(EditorActions& actions, ComponentID component_id, ID id) {
	char desc[50];
	sprintf_s(desc, "DESTROYED COMPONENT %s #i", actions.world.component_type[component_id], id);

	Archetype component_mask = 1ull << component_id;

	EditorActionHeader header{ EditorActionHeader::Destroy_Component, 0, 0, desc };
	EntityCopy* copy = save_entity_copy(actions.world, actions.undo, component_mask, ~component_mask, id, header);
	destroy_components(actions.world, copy);

	actions.frame_diffs.stack.append(header);
}

EditorActionHeader pop_action(ActionStack& stack) {
	EditorActionHeader header = stack.stack.pop();
	stack.head = (stack.block - header.ptr);
	return header;
}

void undo_action(EditorActions& actions) {
	if (actions.undo.stack.length == 0) return;
	
	EditorActionHeader header = pop_action(actions.undo);
	actions.frame_diffs.stack.append(header);

	switch (header.type) {
	case EditorActionHeader::Diff: apply_diff_and_move_to(actions.redo, header, (Diff*)header.ptr); return;
	case EditorActionHeader::Destroy_Component:
	case EditorActionHeader::Destroy_Entity: 
		create_components(actions.world, (EntityCopy*)header.ptr);
		break;
	case EditorActionHeader::Create_Component:
	case EditorActionHeader::Create_Entity: 
		destroy_components(actions.world, (EntityCopy*)header.ptr);
		break;
	}

	copy_onto_stack(actions.redo, header);
}

void redo_action(EditorActions& actions) {
	if (actions.redo.stack.length == 0) return;

	EditorActionHeader header = pop_action(actions.redo);
	actions.frame_diffs.stack.append(header);

	switch (header.type) {
	case EditorActionHeader::Diff: apply_diff_and_move_to(actions.undo, header, (Diff*)header.ptr); return;
	case EditorActionHeader::Destroy_Component:
	case EditorActionHeader::Destroy_Entity:
		destroy_components(actions.world, (EntityCopy*)header.ptr);
		break;

	case EditorActionHeader::Create_Component:
	case EditorActionHeader::Create_Entity:
		create_components(actions.world, (EntityCopy*)header.ptr);
		break;
	}

	copy_onto_stack(actions.undo, header);
}