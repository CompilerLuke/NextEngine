#pragma once
#include "core/core.h"

#include "core/memory/linear_allocator.h"
#include "core/serializer.h"
#include "core/reflection.h"
#include "core/container/array.h"

 void write_EntityNode_to_buffer(struct SerializerBuffer& buffer, struct EntityNode& data); 
 void read_EntityNode_from_buffer(struct DeserializerBuffer& buffer, struct EntityNode& data); 
 refl::Struct* get_EntityNode_type();
