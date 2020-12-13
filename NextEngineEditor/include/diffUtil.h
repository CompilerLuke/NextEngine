#pragma once

#include "core/core.h"
#include "ecs/id.h"
#include "core/reflection.h"
#include "core/container/array.h"
#include "core/container/offset_slice.h"
#include "ecs/ecs.h"
#include "core/reflection.h"
#include "assets/node.h"
#include "ecs/ecs.h"

struct Editor;

struct EditorActionHeader {
    enum ActionType {
        Create_Element,
        Destroy_Element,
        Create_Component,
        Destroy_Component,
        Select,
        Create_Entity,
        Destroy_Entity,
        Diff
    } type;

	char* ptr;
	u64 size;
	sstring name;
};

const uint MAX_SAVED_UNDOS = 40;

struct ActionStack {
	char* block;
	u64 capacity;
	u64 head;
	u64 tail;

	array<MAX_SAVED_UNDOS, EditorActionHeader> stack;
};

struct EditorActions {
    Editor& editor;
	ActionStack frame_diffs;
	ActionStack undo;
	ActionStack redo;
};

struct ElementPtr {
    enum Type {
        Component, Offset, VectorElement, ListerNode, AssetNode, StaticPointer
    } type;
    union {
        uint id;
        struct { uint entity_id; uint component_id; };
        void* ptr;
    };
    refl::Type* refl_type = NULL;
    
    ElementPtr() { memset(this, 0, sizeof(ElementPtr)); }
    ElementPtr(Type type, ID id, refl::Type* refl_type) : type(type), id(id), refl_type(refl_type) {}
    ElementPtr(Type type, ID id, ID component_id, refl::Type* refl_type) : type(type), entity_id(id), component_id(component_id), refl_type(refl_type) {}
    ElementPtr(Type type, void* ptr) : type(type), ptr(ptr) {}
};

struct DiffUtil {
    array<5, ElementPtr> element;
	void* copy_ptr = NULL;
};

struct Diff {
    offset_slice<ElementPtr> element;
	offset_ptr<char> copy_ptr;
	offset_slice<uint> fields_modified;
};

struct SelectionAction {
    enum Type { Entity, AssetNode } type;
    offset_slice<ID> ids;
};

struct EntityCopy {
	struct Component {
		offset_ptr<char> ptr;
		uint component_id;
	};

	Archetype from;
	Archetype to;
	ID id;
	offset_slice<Component> components;
};

struct AssetCopy {
	ID id;
	AssetNode asset_node;
};

void clear_stack(ActionStack&);
void init_actions(EditorActions&);

void begin_diff(DiffUtil&, slice<ElementPtr> ptr, void* copy);
void begin_tdiff(DiffUtil&, slice<ElementPtr> ptr);
bool end_diff(EditorActions&, DiffUtil&, string_view);
void commit_diff(EditorActions&, DiffUtil&);

void begin_e_diff(DiffUtil& util, World& world, ID component_id, ID id, void* copy);
void begin_e_tdiff(DiffUtil& util, World& world, ID component_id, ID id);

template<typename T>
void begin_e_diff(DiffUtil& util, World& world, ID id, T* copy) {
    begin_e_diff(util, world, type_id<T>(), id, copy);
}

template<typename T>
void begin_e_tdiff(DiffUtil& util, World& world, ID id) {
    T* copy = TEMPORARY_ALLOC(T, *world.by_id<T>(id));
    begin_e_diff(util, world, type_id<T>(), id, copy);
}

bool submit_diff(ActionStack& stack, DiffUtil& util, string_view string);

void asset_selection_action(EditorActions& actions, slice<asset_handle> handle);

void entity_selection_action(EditorActions& actions, slice<ID> ids);
void entity_create_action(EditorActions& actions, ID id);
void entity_destroy_action(EditorActions& actions, ID id); //will destroy for you
void entity_create_component_action(EditorActions& actions, uint component_id, ID id); //will create for you
void entity_destroy_component_action(EditorActions& actions, uint component_id, ID id);  //will destroy for you

void undo_action(EditorActions&);
void redo_action(EditorActions&);

void* get_ptr(slice<ElementPtr> elements);

inline ID get_id_of_component(slice<ElementPtr> ptr) {
    for (uint i = 0; i < ptr.length; i++) {
        if (ptr[i].type == ElementPtr::Component) return ptr[i].id;
    }

    return 0;
}

template<typename T>
T* get_component_ptr(slice<ElementPtr> ptr) {
    for (uint i = 0; i < ptr.length; i++) {
        if (ptr[i].type == ElementPtr::Component) {
            if (ptr[i].component_id != type_id<T>()) return nullptr;
            return (T*)get_ptr({ ptr.data + i, ptr.length - i });
        }
    }

    return nullptr;
}
