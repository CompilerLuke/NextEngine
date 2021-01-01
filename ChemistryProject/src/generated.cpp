#include "generated.h"
#include "engine/types.h"
#include "./chemistry_component_ids.h"
#include "./rotating.h"
#include "./gold_foil.h"
#include "./generated.h"
#include "./emission.h"

refl::Struct init_Rotating() {
	refl::Struct type("Rotating", sizeof(Rotating));
	type.fields.append({"speed", offsetof(Rotating, speed), get_float_type()});
	return type;
}

void write_Rotating_to_buffer(SerializerBuffer& buffer, Rotating& data) {
    write_n_to_buffer(buffer, &data, sizeof(Rotating));
}

void read_Rotating_from_buffer(DeserializerBuffer& buffer, Rotating& data) {
    read_n_from_buffer(buffer, &data, sizeof(Rotating));
}

refl::Struct* get_Rotating_type() {
	static refl::Struct type = init_Rotating();
	return &type;
}

refl::Struct init_AlphaParticle() {
	refl::Struct type("AlphaParticle", sizeof(AlphaParticle));
	type.fields.append({"velocity", offsetof(AlphaParticle, velocity), get_vec3_type()});
	type.fields.append({"life", offsetof(AlphaParticle, life), get_float_type()});
	return type;
}

void write_AlphaParticle_to_buffer(SerializerBuffer& buffer, AlphaParticle& data) {
    write_n_to_buffer(buffer, &data, sizeof(AlphaParticle));
}

void read_AlphaParticle_from_buffer(DeserializerBuffer& buffer, AlphaParticle& data) {
    read_n_from_buffer(buffer, &data, sizeof(AlphaParticle));
}

refl::Struct* get_AlphaParticle_type() {
	static refl::Struct type = init_AlphaParticle();
	return &type;
}

refl::Struct init_AlphaEmitter() {
	refl::Struct type("AlphaEmitter", sizeof(AlphaEmitter));
	type.fields.append({"speed", offsetof(AlphaEmitter, speed), get_float_type()});
	type.fields.append({"emission_rate", offsetof(AlphaEmitter, emission_rate), get_float_type()});
	type.fields.append({"alpha_particle_life", offsetof(AlphaEmitter, alpha_particle_life), get_float_type()});
	type.fields.append({"alpha_material", offsetof(AlphaEmitter, alpha_material), get_material_handle_type()});
	return type;
}

void write_AlphaEmitter_to_buffer(SerializerBuffer& buffer, AlphaEmitter& data) {
    write_n_to_buffer(buffer, &data.speed, sizeof(float));
    write_n_to_buffer(buffer, &data.emission_rate, sizeof(float));
    write_n_to_buffer(buffer, &data.alpha_particle_life, sizeof(float));
    write_material_handle_to_buffer(buffer, data.alpha_material);
}

void read_AlphaEmitter_from_buffer(DeserializerBuffer& buffer, AlphaEmitter& data) {
    read_n_from_buffer(buffer, &data.speed, sizeof(float));
    read_n_from_buffer(buffer, &data.emission_rate, sizeof(float));
    read_n_from_buffer(buffer, &data.alpha_particle_life, sizeof(float));
    read_material_handle_from_buffer(buffer, data.alpha_material);
}

refl::Struct* get_AlphaEmitter_type() {
	static refl::Struct type = init_AlphaEmitter();
	return &type;
}

refl::Struct init_Electron() {
	refl::Struct type("Electron", sizeof(Electron));
	type.fields.append({"n", offsetof(Electron, n), get_uint_type()});
	type.fields.append({"target_n", offsetof(Electron, target_n), get_uint_type()});
	type.fields.append({"t", offsetof(Electron, t), get_float_type()});
	return type;
}

void write_Electron_to_buffer(SerializerBuffer& buffer, Electron& data) {
    write_n_to_buffer(buffer, &data, sizeof(Electron));
}

void read_Electron_from_buffer(DeserializerBuffer& buffer, Electron& data) {
    read_n_from_buffer(buffer, &data, sizeof(Electron));
}

refl::Struct* get_Electron_type() {
	static refl::Struct type = init_Electron();
	return &type;
}

refl::Struct init_Zapper() {
	refl::Struct type("Zapper", sizeof(Zapper));
	return type;
}

void write_Zapper_to_buffer(SerializerBuffer& buffer, Zapper& data) {
    write_n_to_buffer(buffer, &data, sizeof(Zapper));
}

void read_Zapper_from_buffer(DeserializerBuffer& buffer, Zapper& data) {
    read_n_from_buffer(buffer, &data, sizeof(Zapper));
}

refl::Struct* get_Zapper_type() {
	static refl::Struct type = init_Zapper();
	return &type;
}

#include "./include/chemistry_component_ids.h"
#include "ecs/ecs.h"
#include "engine/application.h"


APPLICATION_API void register_components(World& world) {
    RegisterComponent components[5] = {};
    components[0].component_id = 25;
    components[0].type = get_Rotating_type();
    components[0].funcs.constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((Rotating*)data + i) Rotating(); };
    components[1].component_id = 26;
    components[1].type = get_AlphaParticle_type();
    components[1].funcs.constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((AlphaParticle*)data + i) AlphaParticle(); };
    components[2].component_id = 27;
    components[2].type = get_AlphaEmitter_type();
    components[2].funcs.constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((AlphaEmitter*)data + i) AlphaEmitter(); };
    components[2].funcs.copy = [](void* dst, void* src, uint count) { for (uint i=0; i<count; i++) new ((AlphaEmitter*)dst + i) AlphaEmitter(((AlphaEmitter*)src)[i]); };
    components[2].funcs.destructor = [](void* ptr, uint count) { for (uint i=0; i<count; i++) ((AlphaEmitter*)ptr)[i].~AlphaEmitter(); };
    components[2].funcs.serialize = [](SerializerBuffer& buffer, void* data, uint count) {
        for (uint i = 0; i < count; i++) write_AlphaEmitter_to_buffer(buffer, ((AlphaEmitter*)data)[i]);
    };
    components[2].funcs.deserialize = [](DeserializerBuffer& buffer, void* data, uint count) {
        for (uint i = 0; i < count; i++) read_AlphaEmitter_from_buffer(buffer, ((AlphaEmitter*)data)[i]);
    };


    components[3].component_id = 28;
    components[3].type = get_Electron_type();
    components[3].funcs.constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((Electron*)data + i) Electron(); };
    components[4].component_id = 29;
    components[4].type = get_Zapper_type();
    components[4].funcs.constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((Zapper*)data + i) Zapper(); };
    world.register_components({components, 5});

};