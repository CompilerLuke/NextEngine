#pragma once
#include "core/core.h"

#include "core/memory/linear_allocator.h"
#include "core/serializer.h"
#include "core/reflection.h"
#include "core/container/array.h"

 refl::Enum* get_Bow_State_type();
 refl::Enum* get_Arrow_State_type();
 void write_Bow_to_buffer(struct SerializerBuffer& buffer, struct Bow& data); 
 void read_Bow_from_buffer(struct DeserializerBuffer& buffer, struct Bow& data); 
 refl::Struct* get_Bow_type();
 void write_Arrow_to_buffer(struct SerializerBuffer& buffer, struct Arrow& data); 
 void read_Arrow_from_buffer(struct DeserializerBuffer& buffer, struct Arrow& data); 
 refl::Struct* get_Arrow_type();
 void write_FPSController_to_buffer(struct SerializerBuffer& buffer, struct FPSController& data); 
 void read_FPSController_from_buffer(struct DeserializerBuffer& buffer, struct FPSController& data); 
 refl::Struct* get_FPSController_type();
 void write_Player_to_buffer(struct SerializerBuffer& buffer, struct Player& data); 
 void read_Player_from_buffer(struct DeserializerBuffer& buffer, struct Player& data); 
 refl::Struct* get_Player_type();
 void write_PlayerInput_to_buffer(struct SerializerBuffer& buffer, struct PlayerInput& data); 
 void read_PlayerInput_from_buffer(struct DeserializerBuffer& buffer, struct PlayerInput& data); 
 refl::Struct* get_PlayerInput_type();
