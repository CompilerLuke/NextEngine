#pragma once
#include "core/core.h"

template<>
struct refl::TypeResolver<Bow::State> {
	 static refl::Type* get();
};

template<>
struct refl::TypeResolver<Arrow::State> {
	 static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct Bow& data); 
template<>
struct refl::TypeResolver<Bow> {
	 static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct Arrow& data); 
template<>
struct refl::TypeResolver<Arrow> {
	 static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct FPSController& data); 
template<>
struct refl::TypeResolver<FPSController> {
	 static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct Player& data); 
template<>
struct refl::TypeResolver<Player> {
	 static refl::Type* get();
};

void write_to_buffer(struct SerializerBuffer& buffer, struct PlayerInput& data); 
template<>
struct refl::TypeResolver<PlayerInput> {
	 static refl::Type* get();
};

