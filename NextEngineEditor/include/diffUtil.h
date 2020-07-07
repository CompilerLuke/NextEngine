#pragma once

#include "ecs/id.h"
#include "core/reflection.h"
#include "core/container/array.h"
#include "ecs/ecs.h"
#include "core/reflection.h"

struct EditorActionHeader {
	enum Type { Diff, Create_Entity, Destroy_Entity, Create_Component, Destroy_Component, Create_Asset, Destroy_Asset } type;
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
	World& world;
	ActionStack frame_diffs;
	ActionStack undo;
	ActionStack redo;
};

struct DiffUtil {
	void* real_ptr = NULL;
	void* copy_ptr = NULL;
	refl::Type* type = NULL;
	uint id = 0;
};

struct Diff {
	void* real_ptr;
	void* copy_ptr;
	refl::Type* type;
	slice<uint> fields_modified;
	uint id;
};

struct EntityCopy {
	struct Component {
		void* ptr;
		uint size;
		ComponentStore* store;
	};

	ID id;
	slice<Component> components;
};

struct AssetCopy {
	ID id;
	AssetNode asset_node;
};

void clear_stack(ActionStack&);
void init_actions(EditorActions&);

void begin_diff(DiffUtil&, void* ptr, void* copy, refl::Type* type);
void begin_tdiff(DiffUtil&, void* ptr, refl::Type* type);
bool end_diff(EditorActions&, DiffUtil&, string_view);
void commit_diff(EditorActions&, DiffUtil&);

bool submit_diff(ActionStack& stack, DiffUtil& util, string_view string);

void entity_create_action(EditorActions& actions, ID id);
void entity_destroy_action(EditorActions& actions, ID id);

template<typename T>
void begin_diff(DiffUtil& util, T* ptr, T* copy) {
	begin_diff(util, ptr, copy, refl_type(T));
}

template<typename T>
void begin_tdiff(DiffUtil& util, T* ptr) {
	begin_tdiff(util, ptr, refl_type(T));
}

void undo_action(EditorActions&);
void redo_action(EditorActions&);

/*
struct Diff : EditorAction {
	void* target;
	void* undo_buffer;
	void* redo_buffer;
	const char* name;

	vector<unsigned int> offsets;
	vector<unsigned int> sizes;
	vector<void*> data;

	void undo() override;
	void redo() override;

	Diff(unsigned int);
	Diff(Diff&&);
	~Diff();
};

struct CreateAction : EditorAction {
	World& world;
	ID id;
	vector<Component> components;
	bool undid = false;

	CreateAction(World&, ID);

	void undo() override;
	void redo() override;

	~CreateAction();
};

struct DestroyAction : CreateAction {
	DestroyAction(World&, ID);

	void undo() override;
	void redo() override;
};

struct DestroyComponentAction : EditorAction {
	ComponentStore* store;
	void* ptr;
	bool undid = false;

	DestroyComponentAction(ComponentStore*, ID);
	~DestroyComponentAction();

	void undo() override;
	void redo() override;
};

struct UndoRedoScope : EditorAction {
	vector<EditorAction*> children;
};
*/

//void submit_action(std::unique_ptr<EditorAction>* action);
//void push_undo(std::unique_ptr<EditorAction>);
//void push_redo(std::unique_ptr<EditorAction>);
//void on_undo();
//void on_redo();
