#pragma once
#include "core/core.h"

void write_to_buffer(struct SerializerBuffer& buffer, struct EntityEditor& data); 
template<>
struct refl::TypeResolver<EntityEditor> {
	 static refl::Type* get();
};

