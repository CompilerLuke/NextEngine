#include "C:\Users\User\source\repos\NextEngine\TheUnpluggingGame\generated.h"
#include "core/memory/linear_allocator.h"
#include "core/serializer.h"
#include "core/reflection.h"
#include "core/container/array.h"
#include "C:\Users\User\source\repos\NextEngine\TheUnpluggingGame\/bowWeapon.h"
#include "C:\Users\User\source\repos\NextEngine\TheUnpluggingGame\/fpsController.h"
#include "C:\Users\User\source\repos\NextEngine\TheUnpluggingGame\/game.h"
#include "C:\Users\User\source\repos\NextEngine\TheUnpluggingGame\/generated.h"
#include "C:\Users\User\source\repos\NextEngine\TheUnpluggingGame\/player.h"
#include "C:\Users\User\source\repos\NextEngine\TheUnpluggingGame\/playerInput.h"
#include "C:\Users\User\source\repos\NextEngine\TheUnpluggingGame\/stdafx.h"
#include "C:\Users\User\source\repos\NextEngine\TheUnpluggingGame\/targetver.h"

refl::Enum init_Bow_State() {
	refl::Enum type("State", sizeof(Bow::State));
	type.values.append({ "Charging", Bow::State::Charging });
	type.values.append({ "Reloading", Bow::State::Reloading });
	type.values.append({ "Firing", Bow::State::Firing });
	return type;
}

 refl::Type* refl::TypeResolver<Bow::State>::get() {
	static refl::Enum type = init_Bow_State();
	return &type;
}
refl::Enum init_Arrow_State() {
	refl::Enum type("State", sizeof(Arrow::State));
	type.values.append({ "Fired", Arrow::State::Fired });
	return type;
}

 refl::Type* refl::TypeResolver<Arrow::State>::get() {
	static refl::Enum type = init_Arrow_State();
	return &type;
}
refl::Struct init_Bow() {
	refl::Struct type("Bow", sizeof(Bow));
	type.fields.append({"attached", offsetof(Bow, attached), refl::TypeResolver<ID>::get() });
	type.fields.append({"state", offsetof(Bow, state), refl::TypeResolver<Bow::State>::get() });
	type.fields.append({"duration", offsetof(Bow, duration), refl::TypeResolver<float>::get() });
	type.fields.append({"arrow_speed", offsetof(Bow, arrow_speed), refl::TypeResolver<float>::get() });
	type.fields.append({"reload_time", offsetof(Bow, reload_time), refl::TypeResolver<float>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, Bow& data) {
    write_to_buffer(buffer, data.attached);
    write_to_buffer(buffer, data.state);
    write_to_buffer(buffer, data.duration);
    write_to_buffer(buffer, data.arrow_speed);
    write_to_buffer(buffer, data.reload_time);
}

 refl::Type* refl::TypeResolver<Bow>::get() {
	static refl::Struct type = init_Bow();
	return &type;
}
refl::Struct init_Arrow() {
	refl::Struct type("Arrow", sizeof(Arrow));
	type.fields.append({"state", offsetof(Arrow, state), refl::TypeResolver<Arrow::State>::get() });
	type.fields.append({"duration", offsetof(Arrow, duration), refl::TypeResolver<float>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, Arrow& data) {
    write_to_buffer(buffer, data.state);
    write_to_buffer(buffer, data.duration);
}

 refl::Type* refl::TypeResolver<Arrow>::get() {
	static refl::Struct type = init_Arrow();
	return &type;
}
refl::Struct init_FPSController() {
	refl::Struct type("FPSController", sizeof(FPSController));
	type.fields.append({"movement_speed", offsetof(FPSController, movement_speed), refl::TypeResolver<float>::get() });
	type.fields.append({"roll_speed", offsetof(FPSController, roll_speed), refl::TypeResolver<float>::get() });
	type.fields.append({"roll_duration", offsetof(FPSController, roll_duration), refl::TypeResolver<float>::get() });
	type.fields.append({"roll_cooldown", offsetof(FPSController, roll_cooldown), refl::TypeResolver<float>::get() });
	type.fields.append({"roll_cooldown_time", offsetof(FPSController, roll_cooldown_time), refl::TypeResolver<float>::get() });
	type.fields.append({"roll_blend", offsetof(FPSController, roll_blend), refl::TypeResolver<float>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, FPSController& data) {
    write_to_buffer(buffer, data.movement_speed);
    write_to_buffer(buffer, data.roll_speed);
    write_to_buffer(buffer, data.roll_duration);
    write_to_buffer(buffer, data.roll_cooldown);
    write_to_buffer(buffer, data.roll_cooldown_time);
    write_to_buffer(buffer, data.roll_blend);
}

 refl::Type* refl::TypeResolver<FPSController>::get() {
	static refl::Struct type = init_FPSController();
	return &type;
}
refl::Struct init_Player() {
	refl::Struct type("Player", sizeof(Player));
	type.fields.append({"health", offsetof(Player, health), refl::TypeResolver<float>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, Player& data) {
    write_to_buffer(buffer, data.health);
}

 refl::Type* refl::TypeResolver<Player>::get() {
	static refl::Struct type = init_Player();
	return &type;
}
refl::Struct init_PlayerInput() {
	refl::Struct type("PlayerInput", sizeof(PlayerInput));
	type.fields.append({"yaw", offsetof(PlayerInput, yaw), refl::TypeResolver<float>::get() });
	type.fields.append({"pitch", offsetof(PlayerInput, pitch), refl::TypeResolver<float>::get() });
	type.fields.append({"vertical_axis", offsetof(PlayerInput, vertical_axis), refl::TypeResolver<float>::get() });
	type.fields.append({"horizonal_axis", offsetof(PlayerInput, horizonal_axis), refl::TypeResolver<float>::get() });
	type.fields.append({"mouse_sensitivity", offsetof(PlayerInput, mouse_sensitivity), refl::TypeResolver<float>::get() });
	type.fields.append({"shift", offsetof(PlayerInput, shift), refl::TypeResolver<bool>::get() });
	type.fields.append({"space", offsetof(PlayerInput, space), refl::TypeResolver<bool>::get() });
	type.fields.append({"holding_mouse_left", offsetof(PlayerInput, holding_mouse_left), refl::TypeResolver<bool>::get() });
	return type;
}

void write_to_buffer(SerializerBuffer& buffer, PlayerInput& data) {
    write_to_buffer(buffer, data.yaw);
    write_to_buffer(buffer, data.pitch);
    write_to_buffer(buffer, data.vertical_axis);
    write_to_buffer(buffer, data.horizonal_axis);
    write_to_buffer(buffer, data.mouse_sensitivity);
    write_to_buffer(buffer, data.shift);
    write_to_buffer(buffer, data.space);
    write_to_buffer(buffer, data.holding_mouse_left);
}

 refl::Type* refl::TypeResolver<PlayerInput>::get() {
	static refl::Struct type = init_PlayerInput();
	return &type;
}
