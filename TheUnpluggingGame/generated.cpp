#include "generated.h"
#include "engine/types.h"
#include "./bowWeapon.h"
#include "./component_ids.h"
#include "./fpsController.h"
#include "./generated.h"
#include "./player.h"
#include "./playerInput.h"
#include "./stdafx.h"

refl::Enum init_Bow_State() {
	refl::Enum type("State", sizeof(Bow::State));
	type.values.append({ "Charging", Bow::State::Charging });
	type.values.append({ "Reloading", Bow::State::Reloading });
	type.values.append({ "Firing", Bow::State::Firing });
	return type;
}

refl::Enum* get_Bow_State_type() {
	static refl::Enum type = init_Bow_State();
	return &type;
}
refl::Enum init_Arrow_State() {
	refl::Enum type("State", sizeof(Arrow::State));
	type.values.append({ "Fired", Arrow::State::Fired });
	return type;
}

refl::Enum* get_Arrow_State_type() {
	static refl::Enum type = init_Arrow_State();
	return &type;
}
refl::Struct init_Bow() {
	refl::Struct type("Bow", sizeof(Bow));
	type.fields.append({"attached", offsetof(Bow, attached), get_ID_type()});
	type.fields.append({"state", offsetof(Bow, state), get_Bow_State_type()});
	type.fields.append({"duration", offsetof(Bow, duration), get_float_type()});
	type.fields.append({"arrow_speed", offsetof(Bow, arrow_speed), get_float_type()});
	type.fields.append({"reload_time", offsetof(Bow, reload_time), get_float_type()});
	return type;
}

void write_Bow_to_buffer(SerializerBuffer& buffer, Bow& data) {
    write_n_to_buffer(buffer, &data, sizeof(Bow));
}

void read_Bow_from_buffer(DeserializerBuffer& buffer, Bow& data) {
    read_n_from_buffer(buffer, &data, sizeof(Bow));
}

refl::Struct* get_Bow_type() {
	static refl::Struct type = init_Bow();
	return &type;
}

refl::Struct init_Arrow() {
	refl::Struct type("Arrow", sizeof(Arrow));
	type.fields.append({"state", offsetof(Arrow, state), get_Arrow_State_type()});
	type.fields.append({"duration", offsetof(Arrow, duration), get_float_type()});
	return type;
}

void write_Arrow_to_buffer(SerializerBuffer& buffer, Arrow& data) {
    write_n_to_buffer(buffer, &data, sizeof(Arrow));
}

void read_Arrow_from_buffer(DeserializerBuffer& buffer, Arrow& data) {
    read_n_from_buffer(buffer, &data, sizeof(Arrow));
}

refl::Struct* get_Arrow_type() {
	static refl::Struct type = init_Arrow();
	return &type;
}

refl::Struct init_FPSController() {
	refl::Struct type("FPSController", sizeof(FPSController));
	type.fields.append({"movement_speed", offsetof(FPSController, movement_speed), get_float_type()});
	type.fields.append({"roll_speed", offsetof(FPSController, roll_speed), get_float_type()});
	type.fields.append({"roll_duration", offsetof(FPSController, roll_duration), get_float_type()});
	type.fields.append({"roll_cooldown", offsetof(FPSController, roll_cooldown), get_float_type()});
	type.fields.append({"roll_cooldown_time", offsetof(FPSController, roll_cooldown_time), get_float_type()});
	type.fields.append({"roll_blend", offsetof(FPSController, roll_blend), get_float_type()});
	return type;
}

void write_FPSController_to_buffer(SerializerBuffer& buffer, FPSController& data) {
    write_n_to_buffer(buffer, &data, sizeof(FPSController));
}

void read_FPSController_from_buffer(DeserializerBuffer& buffer, FPSController& data) {
    read_n_from_buffer(buffer, &data, sizeof(FPSController));
}

refl::Struct* get_FPSController_type() {
	static refl::Struct type = init_FPSController();
	return &type;
}

refl::Struct init_Player() {
	refl::Struct type("Player", sizeof(Player));
	type.fields.append({"health", offsetof(Player, health), get_float_type()});
	return type;
}

void write_Player_to_buffer(SerializerBuffer& buffer, Player& data) {
    write_n_to_buffer(buffer, &data, sizeof(Player));
}

void read_Player_from_buffer(DeserializerBuffer& buffer, Player& data) {
    read_n_from_buffer(buffer, &data, sizeof(Player));
}

refl::Struct* get_Player_type() {
	static refl::Struct type = init_Player();
	return &type;
}

refl::Struct init_PlayerInput() {
	refl::Struct type("PlayerInput", sizeof(PlayerInput));
	type.fields.append({"yaw", offsetof(PlayerInput, yaw), get_float_type()});
	type.fields.append({"pitch", offsetof(PlayerInput, pitch), get_float_type()});
	type.fields.append({"vertical_axis", offsetof(PlayerInput, vertical_axis), get_float_type()});
	type.fields.append({"horizonal_axis", offsetof(PlayerInput, horizonal_axis), get_float_type()});
	type.fields.append({"mouse_sensitivity", offsetof(PlayerInput, mouse_sensitivity), get_float_type()});
	type.fields.append({"shift", offsetof(PlayerInput, shift), get_bool_type()});
	type.fields.append({"space", offsetof(PlayerInput, space), get_bool_type()});
	type.fields.append({"holding_mouse_left", offsetof(PlayerInput, holding_mouse_left), get_bool_type()});
	return type;
}

void write_PlayerInput_to_buffer(SerializerBuffer& buffer, PlayerInput& data) {
    write_n_to_buffer(buffer, &data, sizeof(PlayerInput));
}

void read_PlayerInput_from_buffer(DeserializerBuffer& buffer, PlayerInput& data) {
    read_n_from_buffer(buffer, &data, sizeof(PlayerInput));
}

refl::Struct* get_PlayerInput_type() {
	static refl::Struct type = init_PlayerInput();
	return &type;
}

#include ".//component_ids.h"
#include "ecs/ecs.h"
#include "engine/application.h"


APPLICATION_API void register_components(World& world) {
    RegisterComponent components[5] = {};
    components[0].component_id = 30;
    components[0].type = get_Bow_type();
    components[0].funcs.constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((Bow*)data + i) Bow(); };
    components[1].component_id = 31;
    components[1].type = get_Arrow_type();
    components[1].funcs.constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((Arrow*)data + i) Arrow(); };
    components[2].component_id = 32;
    components[2].type = get_FPSController_type();
    components[2].funcs.constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((FPSController*)data + i) FPSController(); };
    components[3].component_id = 33;
    components[3].type = get_Player_type();
    components[3].funcs.constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((Player*)data + i) Player(); };
    components[4].component_id = 34;
    components[4].type = get_PlayerInput_type();
    components[4].funcs.constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((PlayerInput*)data + i) PlayerInput(); };
    world.register_components({components, 5});

};