#include "diffUtil.h"
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
	if (header.type == EditorActionHeader::Destroy_Entity) {

	}
}

char* stack_push_action(ActionStack& stack, EditorActionHeader& header) {
	uint size = header.size;
	assert(size <= stack.capacity);
	
	//todo extra check, if wrapping!!!!
	if (stack.head + size > stack.capacity) {
		stack.head = 0;

		uint i;
		for (i = 0; i < stack.stack.length; i++) {
			EditorActionHeader header = stack.stack[i];
			stack.tail = (header.ptr - stack.block) + header.size;

			if (stack.tail >= size) break;
		}

		uint shift = i + 1;

		for (uint i = 0; i < shift; i++) {
			destroy_action(stack, stack.stack[i]);
		}

		stack.stack.shift(shift);
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

void begin_tdiff(DiffUtil& util, void* ptr, refl::Type* type) {
	util.real_ptr = ptr;
	util.copy_ptr = temporary_allocator.allocate(type->size);
	util.type = type;
	
	memcpy(util.copy_ptr, util.real_ptr, type->size);
}

void begin_diff(DiffUtil& util, void* ptr, void* copy, refl::Type* type) {
	util.real_ptr = ptr;
	util.copy_ptr = copy;
	util.type = type;

	memcpy(util.copy_ptr, util.real_ptr, type->size);
}

Diff* copy_onto_stack(ActionStack& stack, EditorActionHeader header, const Diff& diff) {
	char* data = stack_push_action(stack, header);
	memset(data, 0, header.size);

	Diff* copy = push_ptr<Diff>(&data);
	copy->id = diff.id;
	copy->copy_ptr = push_ptr<char>(&data, diff.type->size);

	copy->real_ptr = diff.real_ptr;
	copy->type = diff.type;
	copy->fields_modified.length = diff.fields_modified.length;
	copy->fields_modified.data = (uint*)data;

	memcpy(copy->copy_ptr, diff.copy_ptr, copy->type->size);
	memcpy(copy->fields_modified.data, diff.fields_modified.data, diff.fields_modified.length * sizeof(uint));

	return copy;
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

	Diff diff = {};
	diff.id = util.id;
	diff.copy_ptr = util.copy_ptr;
	diff.real_ptr = util.real_ptr;
	diff.type = util.type;
	diff.fields_modified = modified_fields;


	copy_onto_stack(stack, header, diff);

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

EntityCopy* save_entity_copy(World& world, ActionStack& stack, ID id, EditorActionHeader& header) {
	auto components = world.components_by_id(id);

	uint size = sizeof(EntityCopy) + sizeof(Component) * components.length;
	
	
	//for (Component& component : components) {
	//	size += component.type->size;
	//	size += sizeof(Component);
	//}

	header.size = size;

	char* data = stack_push_action(stack, header);

	EntityCopy* entity_copy = push_ptr<EntityCopy>(&data);
	entity_copy->id = id;

	entity_copy->components.data = push_ptr<EntityCopy::Component>(&data, components.length);
	entity_copy->components.length = components.length;

	printf("=============\n");

	for (uint i = 0; i < components.length; i++) {
		EntityCopy::Component& component = entity_copy->components[i];
		component.store = components[i].store;
		component.ptr = components[i].data; //push_ptr<char>(&data, components[i].type->size);
		component.size = components[i].type->size;

		printf("Ptr of save entity copy %p\n", component.ptr);

		//memcpy(component.ptr, components[i].data, components[i].type->size);
	}

	return entity_copy;
}

void copy_to_stack(ActionStack& stack, EditorActionHeader& header, EntityCopy& master) {
	char* data = stack_push_action(stack, header);
	memcpy(data, &master, header.size);

	//Fixup pointers
	EntityCopy* copy = push_ptr<EntityCopy>(&data);
	copy->components.data = push_ptr<EntityCopy::Component>(&data, master.components.length);

	/*for (uint i = 0; i < master.components.length; i++) {
		EntityCopy::Component& component = copy->components[i];
		copy->components[i].ptr = push_ptr<char>(&data, component.size);
	}*/
}

void apply_diff_and_move_to(ActionStack& actions, EditorActionHeader& header, Diff* diff) {
	Diff* copy = copy_onto_stack(actions, header, *diff);

	memcpy(copy->copy_ptr, diff->real_ptr, diff->type->size);
	memcpy(diff->real_ptr, diff->copy_ptr, diff->type->size);
}

void destroy_all_components(EntityCopy* copy) {
	for (EntityCopy::Component component : copy->components) {
		printf("DESTROY component %p\n", component.ptr);
		component.store->set_enabled(component.ptr, false);
	}
}

void create_all_components(EntityCopy* copy) {
	for (EntityCopy::Component component : copy->components) {
		printf("CREATE component %p\n", component.ptr);
		component.store->set_enabled(component.ptr, true);
		//void* ptr = component.store->make_by_id(copy->id);
		//memcpy(ptr, component.ptr, component.size);
	}
}

void entity_create_action(EditorActions& actions, ID id) {
	char desc[50];
	sprintf_s(desc, "CREATED ENTITY #%i", id);

	EditorActionHeader header{ EditorActionHeader::Create_Entity , 0, 0, desc};
	save_entity_copy(actions.world, actions.undo, id, header);
	actions.frame_diffs.stack.append(header);
}

void entity_destroy_action(EditorActions& actions, ID id) {
	char desc[50];
	sprintf_s(desc, "DESTROYED ENTITY #%i", id);
	
	EditorActionHeader header{ EditorActionHeader::Destroy_Entity, 0, 0, desc };
	EntityCopy* copy = save_entity_copy(actions.world, actions.undo, id, header);
	destroy_all_components(copy);

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

	switch (header.type) {
	case EditorActionHeader::Diff: apply_diff_and_move_to(actions.redo, header, (Diff*)header.ptr); break;
	case EditorActionHeader::Destroy_Entity: 
		create_all_components((EntityCopy*)header.ptr);
		copy_to_stack(actions.redo, header, *(EntityCopy*)header.ptr);
		break;
	case EditorActionHeader::Create_Entity: 
		destroy_all_components((EntityCopy*)header.ptr);
		copy_to_stack(actions.redo, header, *(EntityCopy*)header.ptr);
		break;
	}

	actions.frame_diffs.stack.append(header);
}

void redo_action(EditorActions& actions) {
	if (actions.redo.stack.length == 0) return;

	EditorActionHeader header = pop_action(actions.redo);

	switch (header.type) {
	case EditorActionHeader::Diff: apply_diff_and_move_to(actions.undo, header, (Diff*)header.ptr); break;
	case EditorActionHeader::Destroy_Entity:
		destroy_all_components((EntityCopy*)header.ptr);
		copy_to_stack(actions.undo, header, *(EntityCopy*)header.ptr);
		break;

	case EditorActionHeader::Create_Entity:
		create_all_components((EntityCopy*)header.ptr);
		copy_to_stack(actions.undo, header, *(EntityCopy*)header.ptr);
		break;
	}

	actions.frame_diffs.stack.append(header);
}




/*
CreateAction::CreateAction(World& world, ID id) 
: world(world), id(id) {
	auto components = world.components_by_id(id);

	this->components.reserve(components.length); //components by id is only temporarily valid
	for (int i = 0; i < components.length; i++)
		this->components.append(components[i]);
}

void CreateAction::undo() {
	undid = true;
	for (auto& comp : this->components) {
		comp.store->set_enabled(comp.data, false);
	}
}

void CreateAction::redo() {
	undid = false;
	for (auto& comp : this->components) {
		comp.store->set_enabled(comp.data, true);
	}
}

CreateAction::~CreateAction() {
	if (undid) {
		world.free_by_id(id);
	}
}

DestroyAction::DestroyAction(World& w, ID id) : CreateAction(w, id) {
	CreateAction::undo();
}

void DestroyAction::redo() {
	CreateAction::undo();
}

void DestroyAction::undo() {
	CreateAction::redo();
}

DestroyComponentAction::DestroyComponentAction(ComponentStore* store, ID id) {
	this->store = store;
	this->ptr = store->get_by_id(id).data;

	redo();
}

void DestroyComponentAction::undo() {
	undid = true;
	store->set_enabled(ptr, true);
}

void DestroyComponentAction::redo() {
	undid = false;
	store->set_enabled(ptr, false);
}

DestroyComponentAction::~DestroyComponentAction() {
	if (!undid) {
		store->free_by_id(((Slot<char>*)ptr)->object.second);
	}
}
*/