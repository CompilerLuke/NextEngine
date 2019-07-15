#pragma once

#include "ecs/id.h"
#include "reflection/reflection.h"
#include "core/vector.h"

struct DiffUtil {
	void* real_ptr;
	void* copy_ptr;
	reflect::TypeDescriptor* type;

	void submit(struct Editor&, const char*);

	DiffUtil(void*, reflect::TypeDescriptor* type);
};

struct Diff {
	void* buffer;

	vector<unsigned int> offsets;
	vector<void*> data;

	Diff(unsigned int);
	~Diff();
};