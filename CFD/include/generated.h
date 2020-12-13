#pragma once
#include "core/core.h"

#include "core/memory/linear_allocator.h"
#include "core/serializer.h"
#include "core/reflection.h"
#include "core/container/array.h"

 void write_CFDMesh_to_buffer(struct SerializerBuffer& buffer, struct CFDMesh& data); 
 void read_CFDMesh_from_buffer(struct DeserializerBuffer& buffer, struct CFDMesh& data); 
 refl::Struct* get_CFDMesh_type();
 void write_CFDDomain_to_buffer(struct SerializerBuffer& buffer, struct CFDDomain& data); 
 void read_CFDDomain_from_buffer(struct DeserializerBuffer& buffer, struct CFDDomain& data); 
 refl::Struct* get_CFDDomain_type();
