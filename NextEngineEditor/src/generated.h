#pragma once
#include "core/core.h"

void write_to_buffer(struct SerializerBuffer& buffer, struct EntityNode& data); 
template<>
struct refl::TypeResolver<EntityNode> {
	 static refl::Type* get();
};

