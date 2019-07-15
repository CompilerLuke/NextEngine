#pragma once

#include "ecs/id.h"
#include "reflection/reflection.h"

struct DiffUtil {
	void* ptr;
	reflect::TypeDescriptor* type;

	void submit(struct Editor&, struct World&);

	DiffUtil(void*, reflect::TypeDescriptor* type);
};