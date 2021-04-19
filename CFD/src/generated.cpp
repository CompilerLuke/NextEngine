#include "generated.h"
#include "engine/types.h"
#include "./cfd_components.h"

refl::Struct init_input_model_handle() {
	refl::Struct type("input_model_handle", sizeof(input_model_handle));
	type.fields.append({"id", offsetof(input_model_handle, id), get_uint_type()});
	return type;
}

void write_input_model_handle_to_buffer(SerializerBuffer& buffer, input_model_handle& data) {
    write_n_to_buffer(buffer, &data, sizeof(input_model_handle));
}

void read_input_model_handle_from_buffer(DeserializerBuffer& buffer, input_model_handle& data) {
    read_n_from_buffer(buffer, &data, sizeof(input_model_handle));
}

refl::Struct* get_input_model_handle_type() {
	static refl::Struct type = init_input_model_handle();
	return &type;
}

refl::Struct init_CFDMesh() {
	refl::Struct type("CFDMesh", sizeof(CFDMesh));
	type.fields.append({"concave", offsetof(CFDMesh, concave), get_bool_type()});
	type.fields.append({"model", offsetof(CFDMesh, model), get_input_model_handle_type()});
	type.fields.append({"color", offsetof(CFDMesh, color), get_vec4_type()});
	return type;
}

void write_CFDMesh_to_buffer(SerializerBuffer& buffer, CFDMesh& data) {
    write_n_to_buffer(buffer, &data, sizeof(CFDMesh));
}

void read_CFDMesh_from_buffer(DeserializerBuffer& buffer, CFDMesh& data) {
    read_n_from_buffer(buffer, &data, sizeof(CFDMesh));
}

refl::Struct* get_CFDMesh_type() {
	static refl::Struct type = init_CFDMesh();
	return &type;
}

refl::Struct init_CFDDomain() {
	refl::Struct type("CFDDomain", sizeof(CFDDomain));
	type.fields.append({"size", offsetof(CFDDomain, size), get_vec3_type()});
	type.fields.append({"contour_layers", offsetof(CFDDomain, contour_layers), get_int_type()});
	type.fields.append({"contour_initial_thickness", offsetof(CFDDomain, contour_initial_thickness), get_float_type()});
	type.fields.append({"contour_thickness_expontent", offsetof(CFDDomain, contour_thickness_expontent), get_float_type()});
	type.fields.append({"tetrahedron_layers", offsetof(CFDDomain, tetrahedron_layers), get_int_type()});
	type.fields.append({"grid_resolution", offsetof(CFDDomain, grid_resolution), get_float_type()});
	type.fields.append({"grid_layers", offsetof(CFDDomain, grid_layers), get_int_type()});
	type.fields.append({"center", offsetof(CFDDomain, center), get_vec3_type()});
	type.fields.append({"plane", offsetof(CFDDomain, plane), get_vec3_type()});
	type.fields.append({"quad_quality", offsetof(CFDDomain, quad_quality), get_float_type()});
	type.fields.append({"feature_angle", offsetof(CFDDomain, feature_angle), get_float_type()});
	type.fields.append({"min_feature_quality", offsetof(CFDDomain, min_feature_quality), get_float_type()});
	return type;
}

void write_CFDDomain_to_buffer(SerializerBuffer& buffer, CFDDomain& data) {
    write_n_to_buffer(buffer, &data, sizeof(CFDDomain));
}

void read_CFDDomain_from_buffer(DeserializerBuffer& buffer, CFDDomain& data) {
    read_n_from_buffer(buffer, &data, sizeof(CFDDomain));
}

refl::Struct* get_CFDDomain_type() {
	static refl::Struct type = init_CFDDomain();
	return &type;
}

#include "./include/cfd_ids.h"
#include "ecs/ecs.h"
#include "engine/application.h"


APPLICATION_API void register_components(World& world) {
    RegisterComponent components[2] = {};
    components[0].component_id = 30;
    components[0].type = get_CFDMesh_type();
    components[0].funcs.constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((CFDMesh*)data + i) CFDMesh(); };
    components[1].component_id = 31;
    components[1].type = get_CFDDomain_type();
    components[1].funcs.constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((CFDDomain*)data + i) CFDDomain(); };
    world.register_components({components, 2});

};