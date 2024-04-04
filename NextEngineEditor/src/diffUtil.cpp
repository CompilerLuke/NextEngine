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
    if (stack.head >= stack.tail) return stack.head + size <= stack.capacity;
    else stack.head + size <= stack.tail;
}

char* stack_push_action(ActionStack& stack, EditorActionHeader& header) {
    uint size = header.size;
    assert(size <= stack.capacity);

    if (stack.stack.length == MAX_SAVED_UNDOS || !enough_space_in_stack(stack, size)) {
        printf("Not sufficient space in stack %i, head %i, tail %i", stack.head, stack.tail);
        if (stack.head > stack.tail) stack.head = 0;

        uint i;
        for (i = 0; i < stack.stack.length; i++) {
            EditorActionHeader header = stack.stack[i];
            stack.tail = (header.ptr - stack.block) + header.size;

            if (enough_space_in_stack(stack, size)) break;
        }

        uint shift = min(i + 1, stack.stack.length);

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

#include "lister.h"

void* get_ptr(slice<ElementPtr> elements) {
    uint last = elements.length - 1;
    assert(elements[last].type == ElementPtr::StaticPointer);
    
    void* ptr = nullptr;
    
    for (int i = last; i >= 0; i--) {
        ElementPtr element = elements[i];
        
        switch (element.type) {
            case ElementPtr::Component: {
                World& world = *(World*)ptr;
                ptr = world.id_to_ptr[element.component_id][element.id];
                break;
            }
            
            case ElementPtr::VectorElement: {
                vector<char>& vec = *(vector<char>*)ptr;
                assert(element.id < vec.length);
                ptr = vec.data + element.refl_type->size * element.id;
                break;
            }
                
            case ElementPtr::ListerNode: {
                Lister& lister = *(Lister*)ptr;
                ptr = node_by_id(lister, element.id);
                break;
            }
                
            case ElementPtr::AssetNode: {
                AssetInfo& info = *(AssetInfo*)ptr;
                ptr = &info.asset_type_handle_to_node[element.component_id][element.id]->texture;
                break;
            }
                
            case ElementPtr::Offset:
                ptr = (char*)ptr + element.id;
                break;
                
            case ElementPtr::StaticPointer:
                ptr = (char*)ptr;
                if (element.refl_type) ptr = (char*)element.ptr + element.refl_type->size * element.id;
                else ptr = element.ptr;
                break;
        }
    }
    
    return ptr;
}

void begin_diff(DiffUtil& util, slice<ElementPtr> element, void* copy) {
    util.element = element;
    util.copy_ptr = copy;
    memcpy(copy, get_ptr(element), element[0].refl_type->size);
}

void begin_tdiff(DiffUtil& util, slice<ElementPtr> ptr) {
    u64 size = ptr[0].refl_type->size;
    void* copy_ptr = get_temporary_allocator().allocate(size);
    begin_diff(util, ptr, copy_ptr);
}

void begin_e_diff(DiffUtil& util, World& world, ID component_id, ID id, void* copy) {
    ElementPtr ptr[2] = {
        {ElementPtr::Component, id, component_id, world.component_type[component_id]},
        {ElementPtr::StaticPointer, &world}
    };
    
    begin_diff(util, {ptr, 2}, copy);
}

void begin_e_tdiff(DiffUtil& util, World& world, ID component_id, ID id) {
    uint size = world.component_size[component_id];
    void* ptr = get_temporary_allocator().allocate(size);
    begin_e_diff(util, world, component_id, id, ptr);
}

bool find_modified_fields(tvector<uint>& modified_fields, refl::Type* diff_type, void* copy_ptr, void* real_ptr) {
    bool is_modified = false;
    
    assert(diff_type->type == refl::Type::Struct || diff_type->type == refl::Type::Union);
    assert(diff_type->size < 1000);

    if (diff_type->type == refl::Type::Struct) {
        refl::Struct* type = (refl::Struct*)diff_type;

        for (uint i = 0; i < type->fields.length; i++) {
            auto& member = type->fields[i];
            auto original_value = (char*)copy_ptr + member.offset;
            auto new_value = (char*)real_ptr + member.offset;

            if (memcmp(new_value, original_value, member.type->size) != 0) {
                modified_fields.append(i);
                is_modified = true;
            }
        }
    }
    else if (diff_type->type == refl::Type::Union) {
        is_modified = memcmp(copy_ptr, real_ptr, diff_type->size) != 0;
    }
    
    return is_modified;
}


bool submit_diff(ActionStack& stack, DiffUtil& util,  string_view string) {
    void* real_ptr = get_ptr(util.element);
    refl::Type* type = util.element[0].refl_type;

    tvector<uint> modified_fields;
	if (!find_modified_fields(modified_fields, type, util.copy_ptr, real_ptr)) return false;

	EditorActionHeader header = {};
	header.type = EditorActionHeader::Diff;
	header.name = string;
	header.size = sizeof(Diff) + type->size + sizeof(uint) * modified_fields.length + sizeof(ElementPtr) * util.element.length;

	char* buffer = stack_push_action(stack, header);
	Diff* diff = push_ptr<Diff>(&buffer);
    diff->element = copy_slice<ElementPtr>(&buffer, util.element);
	diff->copy_ptr = copy_ptr<char>(&buffer, (char*)util.copy_ptr, type->size);
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

		printf("Ptr of save entity copy %p\n", component.ptr.data());

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

    void* real_ptr = get_ptr(diff->element);
    refl::Type* type = diff->element[0].refl_type;
    
	memcpy(copy->copy_ptr.data(), real_ptr, type->size);
	memcpy(real_ptr, diff->copy_ptr.data(), type->size);
    
    printf("Applying diff to entity %p!\n", real_ptr);
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
	snprintf(desc, 50, "CREATED ENTITY #%i", id);

	EditorActionHeader header{ EditorActionHeader::Create_Entity , 0, 0, desc};
	save_entity_copy(actions.editor.world, actions.undo, ANY_ARCHETYPE, 0, id, header);
	actions.frame_diffs.stack.append(header);
}

void entity_destroy_action(EditorActions& actions, ID id) {
	char desc[50];
	snprintf(desc, 50, "DESTROYED ENTITY #%i", id);
	
	EditorActionHeader header{ EditorActionHeader::Destroy_Entity, 0, 0, desc };
	EntityCopy* copy = save_entity_copy(actions.editor.world, actions.undo, ANY_ARCHETYPE, 0, id, header);
	destroy_components(actions.editor.world, copy);

	actions.frame_diffs.stack.append(header);
}

void entity_create_component_action(EditorActions& actions, ComponentID component_id, ID id) {
    World& world = actions.editor.world;
    
    char desc[50];
	snprintf(desc, 50, "CREATED COMPONENT %s #i", world.component_type[component_id], id);
    
	Archetype component_mask = 1ull << component_id;
	Archetype arch = world.arch_of_id(id);

	world.change_archetype(id, arch, arch | component_mask, true);

	EditorActionHeader header{ EditorActionHeader::Create_Component, 0, 0, desc };
	EntityCopy* copy = save_entity_copy(world, actions.undo, component_mask, ~component_mask, id, header);

	actions.frame_diffs.stack.append(header);
}

void entity_destroy_component_action(EditorActions& actions, ComponentID component_id, ID id) {
    World& world = actions.editor.world;
    
    char desc[50];
	snprintf(desc, 50, "DESTROYED COMPONENT %s #%i", world.component_type[component_id], id);

	Archetype component_mask = 1ull << component_id;

	EditorActionHeader header{ EditorActionHeader::Destroy_Component, 0, 0, desc };
	EntityCopy* copy = save_entity_copy(world, actions.undo, component_mask, ~component_mask, id, header);
	destroy_components(world, copy);

	actions.frame_diffs.stack.append(header);
}

void selection_action(EditorActions& actions, SelectionAction::Type type, const char* name, slice<ID> ids) {
    EditorActionHeader header = {};
    header.type = EditorActionHeader::Select;
    header.size = sizeof(SelectionAction) + sizeof(ID) * ids.length;
    header.name = name;
 
    char* buffer = stack_push_action(actions.undo, header);
    SelectionAction* action = push_ptr<SelectionAction>(&buffer);
    action->type = type;
    action->ids = copy_slice(&buffer, ids);
}

void entity_selection_action(EditorActions& actions, slice<ID> ids) {
    selection_action(actions, SelectionAction::Entity, "Entity Selection", ids);
}

void entity_selection_action(EditorActions& actions, slice<asset_handle> ids) {
    selection_action(actions, SelectionAction::AssetNode, "Asset Selection", {(ID*)ids.data, ids.length});
}

void apply_selection_and_move_to(Editor& editor, ActionStack& actions, EditorActionHeader header) {
    SelectionAction* action = (SelectionAction*)header.ptr;
    
    ID currently_selected_id;
    slice<ID> currently_selected;

    if (action->type == SelectionAction::Entity) {
        if (editor.selected_id != -1) {
            currently_selected_id = editor.selected_id;
            currently_selected = currently_selected_id;
        }
    } else {
        if (!editor.asset_tab.explorer.selected.id) {
            currently_selected_id = editor.selected_id;
            currently_selected = currently_selected_id;
        }
    }
    
    EditorActionHeader new_header = header;
    new_header.size = sizeof(SelectionAction) + sizeof(uint) * currently_selected.length;
    char* buffer = stack_push_action(actions, new_header);
    SelectionAction* new_action = push_ptr<SelectionAction>(&buffer);
    new_action->type = action->type;
    new_action->ids = copy_slice(&buffer, currently_selected);
    
    if (action->type == SelectionAction::Entity) {
        editor.selected_id = action->ids.length == 0 ? -1 : action->ids[0];
    } else {
        editor.asset_tab.explorer.selected = action->ids.length == 0 ? asset_handle{0} : asset_handle{action->ids[0]};
    }
    
    printf("Applying selection");
}

EditorActionHeader pop_action(ActionStack& stack) {
	EditorActionHeader header = stack.stack.pop();
	stack.head = header.ptr - stack.block;
	return header;
}

void undo_action(EditorActions& actions) {
	if (actions.undo.stack.length == 0) return;
	
	EditorActionHeader header = pop_action(actions.undo);
	actions.frame_diffs.stack.append(header);
    
    World& world = actions.editor.world;

	switch (header.type) {
    case EditorActionHeader::Select: apply_selection_and_move_to(actions.editor, actions.redo, header); return;
	case EditorActionHeader::Diff: apply_diff_and_move_to(actions.redo, header, (Diff*)header.ptr); return;
    case EditorActionHeader::Destroy_Component:
	case EditorActionHeader::Destroy_Entity: 
		create_components(world, (EntityCopy*)header.ptr);
		break;
	case EditorActionHeader::Create_Component:
	case EditorActionHeader::Create_Entity: 
		destroy_components(world, (EntityCopy*)header.ptr);
		break;
	}

	copy_onto_stack(actions.redo, header);
}

void redo_action(EditorActions& actions) {
	if (actions.redo.stack.length == 0) return;

	EditorActionHeader header = pop_action(actions.redo);
	actions.frame_diffs.stack.append(header);
    
    World& world = actions.editor.world;

	switch (header.type) {
    case EditorActionHeader::Select: apply_selection_and_move_to(actions.editor, actions.undo, header); return;
	case EditorActionHeader::Diff: apply_diff_and_move_to(actions.undo, header, (Diff*)header.ptr); return;
	case EditorActionHeader::Destroy_Component:
	case EditorActionHeader::Destroy_Entity:
		destroy_components(world, (EntityCopy*)header.ptr);
		break;

	case EditorActionHeader::Create_Component:
	case EditorActionHeader::Create_Entity:
		create_components(world, (EntityCopy*)header.ptr);
		break;
    
    case EditorActionHeader::Create_Element: abort();
    case EditorActionHeader::Destroy_Element: abort();
	}

	copy_onto_stack(actions.undo, header);
}
