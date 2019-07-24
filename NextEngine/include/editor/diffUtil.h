#pragma once

#include "ecs/id.h"
#include "reflection/reflection.h"
#include "core/vector.h"
#include "ecs/ecs.h"

struct EditorAction {
	virtual void undo() {};
	virtual void redo() {};
	virtual ~EditorAction() {};
};

struct DiffUtil {
	void* real_ptr;
	void* copy_ptr;
	const char* name;
	Allocator* allocator;

	reflect::TypeDescriptor* type;

	bool submit(struct Editor&, const char*);

	DiffUtil(void*, reflect::TypeDescriptor* type, Allocator* allocator);

	template<typename T>
	DiffUtil(T* ptr, Allocator* allocator) : DiffUtil(ptr, reflect::TypeResolver<T>::get(), allocator) {}
	//todo add templatized version which automatically gets type
};

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