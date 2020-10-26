#include "stdafx.h"
#include "core/memory/linear_allocator.h"
#include "core/reflection.h"
#include "core/container/vector.h"
#include "core/container/tvector.h"
#include "core/container/array.h"
#include "core/container/sstring.h"
#include "core/container/string_buffer.h"
#include <glm/gtc/quaternion.hpp>

using namespace refl;

LinearAllocator reflection_allocator(mb(10));

#define RESOLVE_PRIMITIVE_TYPE(typ, enum_name) Type* get_##typ##_type() { \
	static Type type{Type::enum_name, sizeof(typ) };\
	return &type;\
} 

#define RESOLVE_TYPE(typ, name) Type* get_##name##_type() { \
	static auto type = init_##name(); \
	return &type; \
} \

RESOLVE_PRIMITIVE_TYPE(uint, UInt)
	RESOLVE_PRIMITIVE_TYPE(float, Float)
	RESOLVE_PRIMITIVE_TYPE(int, Int)
	RESOLVE_PRIMITIVE_TYPE(u64, UInt)
	RESOLVE_PRIMITIVE_TYPE(bool, Bool)
	RESOLVE_PRIMITIVE_TYPE(string_view, StringView)

	Struct init_vec2() {
	Struct type("glm::vec2", sizeof(glm::vec2));
	type.fields.append({ "x", offsetof(glm::vec2, x), get_float_type() });
	type.fields.append({ "y", offsetof(glm::vec2, y), get_float_type() });

	return type;
}

RESOLVE_TYPE(glm::vec2, vec2)

	Struct init_vec3() {
	Struct type("glm::vec3", sizeof(glm::vec3));
	type.fields.append({ "x", offsetof(glm::vec3, x), get_float_type() });
	type.fields.append({ "y", offsetof(glm::vec3, y), get_float_type() });
	type.fields.append({ "z", offsetof(glm::vec3, z), get_float_type() });

	return type;
}

RESOLVE_TYPE(glm::vec3, vec3)

	Struct init_vec4() {
	Struct type("glm::vec4", sizeof(glm::vec4));
	type.fields.append({ "x", offsetof(glm::vec4, x), get_float_type() });
	type.fields.append({ "y", offsetof(glm::vec4, y), get_float_type() });
	type.fields.append({ "z", offsetof(glm::vec4, z), get_float_type() });
	type.fields.append({ "w", offsetof(glm::vec4, w), get_float_type() });

	return type;
}

Struct init_quat() {
	Struct type("glm::quat", sizeof(glm::quat));
	type.fields.append({ "x", offsetof(glm::quat, x), get_float_type() });
	type.fields.append({ "y", offsetof(glm::quat, y), get_float_type() });
	type.fields.append({ "z", offsetof(glm::quat, z), get_float_type() });
	type.fields.append({ "w", offsetof(glm::quat, w), get_float_type() });

	return type;
}

RESOLVE_TYPE(glm::quat, quat)

Struct init_ivec2() {
	Struct type("glm::ivec2", sizeof(glm::ivec2));
	type.fields.append({ "x", offsetof(glm::ivec2, x), get_int_type() });
	type.fields.append({ "y", offsetof(glm::ivec2, y), get_int_type() });

	return type;
}

RESOLVE_TYPE(glm::ivec2, ivec2)

Struct init_mat4() {
	Struct type("glm::mat4", sizeof(glm::mat4));
	return type;
}

RESOLVE_TYPE(glm::mat4, mat4)

Type init_sstring() {
	return Type{Type::SString, sizeof(sstring)};
}

RESOLVE_TYPE(sstring, sstring)

Type init_string_buffer() {
	return Type{ Type::StringBuffer, sizeof(string_buffer) };
}

RESOLVE_TYPE(string_buffer, string_buffer)

Array* make_vector_type(Type* type) {
	char* name = PERMANENT_ARRAY(char, 100);
	sprintf_s(name, 100, "vector<%s>", type->name.c_str());

	return PERMANENT_ALLOC(Array, Array::Vector, sizeof(vector<char>), name, type);
}

Array* make_tvector_type(Type* type) {
	char* name = PERMANENT_ARRAY(char, 100);
	sprintf_s(name, 100, "tvector<%s>", type->name.c_str());

	return PERMANENT_ALLOC(Array, Array::TVector, sizeof(tvector<char>), name, type);
}

Array* make_array_type(uint N, uint size, Type* type) {
	char* name = PERMANENT_ARRAY(char, 100);
	sprintf_s(name, 100, "array<%i, %s>", N, type->name.c_str());

	return PERMANENT_ALLOC(Array, Array::StaticArray, size, name, type);
}

Array* make_carray_type(uint N, Type* type) {
	char* name = PERMANENT_ARRAY(char, 100);
	sprintf_s(name, 100, "%s[%i]", type->name.c_str(), N);

	return PERMANENT_ALLOC(Array, Array::CArray, sizeof(void*) + type->size * N, name, type);
}
