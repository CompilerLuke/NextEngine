#pragma once
#include "core/core.h"

#include "core/memory/linear_allocator.h"
#include "core/serializer.h"
#include "core/reflection.h"
#include "core/container/array.h"

 void write_Rotating_to_buffer(struct SerializerBuffer& buffer, struct Rotating& data); 
 void read_Rotating_from_buffer(struct DeserializerBuffer& buffer, struct Rotating& data); 
 refl::Struct* get_Rotating_type();
 void write_AlphaParticle_to_buffer(struct SerializerBuffer& buffer, struct AlphaParticle& data); 
 void read_AlphaParticle_from_buffer(struct DeserializerBuffer& buffer, struct AlphaParticle& data); 
 refl::Struct* get_AlphaParticle_type();
 void write_AlphaEmitter_to_buffer(struct SerializerBuffer& buffer, struct AlphaEmitter& data); 
 void read_AlphaEmitter_from_buffer(struct DeserializerBuffer& buffer, struct AlphaEmitter& data); 
 refl::Struct* get_AlphaEmitter_type();
 void write_Electron_to_buffer(struct SerializerBuffer& buffer, struct Electron& data); 
 void read_Electron_from_buffer(struct DeserializerBuffer& buffer, struct Electron& data); 
 refl::Struct* get_Electron_type();
 void write_Zapper_to_buffer(struct SerializerBuffer& buffer, struct Zapper& data); 
 void read_Zapper_from_buffer(struct DeserializerBuffer& buffer, struct Zapper& data); 
 refl::Struct* get_Zapper_type();
