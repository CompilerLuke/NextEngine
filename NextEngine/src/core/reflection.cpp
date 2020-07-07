//
//  reflection_basic_types.cpp
//  PixC
//
//  Created by Lucas Goetz on 08/11/2019.
//  Copyright © 2019 Lucas Goetz. All rights reserved.
//

#include "core/memory/linear_allocator.h"
#include "core/reflection.h"
#include "core/container/vector.h"
#include "core/container/tvector.h"
#include "core/container/array.h"
#include "core/container/sstring.h"
#include "core/container/string_buffer.h"
#include <glm/gtc/quaternion.hpp>

namespace refl {
	LinearAllocator reflection_allocator(mb(10));

#define RESOLVE_PRIMITIVE_TYPE(typ, enum_name) template<> struct TypeResolver<typ> { \
	ENGINE_API static Type* get() {\
		static Type type{Type::enum_name, sizeof(typ) };\
		return &type;\
	} \
};

#define RESOLVE_TYPE(typ, func) template<> struct TypeResolver<typ> { \
	ENGINE_API static Type* get() { \
		static auto type = func(); \
		return &type; \
	} \
};

	RESOLVE_PRIMITIVE_TYPE(uint, UInt)
		RESOLVE_PRIMITIVE_TYPE(float, Float)
		RESOLVE_PRIMITIVE_TYPE(int, Int)
		RESOLVE_PRIMITIVE_TYPE(u64, UInt)
		RESOLVE_PRIMITIVE_TYPE(bool, Bool)
		RESOLVE_PRIMITIVE_TYPE(string_view, StringView)

		Struct init_vec2() {
		Struct type("glm::vec2", sizeof(glm::vec2));
		type.fields.append({ "x", offsetof(glm::vec2, x), refl_type(float) });
		type.fields.append({ "y", offsetof(glm::vec2, y), refl_type(float) });

		return type;
	}

	RESOLVE_TYPE(glm::vec2, init_vec2)

		Struct init_vec3() {
		Struct type("glm::vec3", sizeof(glm::vec3));
		type.fields.append({ "x", offsetof(glm::vec3, x), refl_type(float) });
		type.fields.append({ "y", offsetof(glm::vec3, y), refl_type(float) });
		type.fields.append({ "z", offsetof(glm::vec3, z), refl_type(float) });

		return type;
	}

	RESOLVE_TYPE(glm::vec3, init_vec3)

		Struct init_vec4() {
		Struct type("glm::vec4", sizeof(glm::vec4));
		type.fields.append({ "x", offsetof(glm::vec4, x), refl_type(float) });
		type.fields.append({ "y", offsetof(glm::vec4, y), refl_type(float) });
		type.fields.append({ "z", offsetof(glm::vec4, z), refl_type(float) });
		type.fields.append({ "w", offsetof(glm::vec4, w), refl_type(float) });

		return type;
	}

	Struct init_quat() {
		Struct type("glm::quat", sizeof(glm::quat));
		type.fields.append({ "x", offsetof(glm::quat, x), refl_type(float) });
		type.fields.append({ "y", offsetof(glm::quat, y), refl_type(float) });
		type.fields.append({ "z", offsetof(glm::quat, z), refl_type(float) });
		type.fields.append({ "w", offsetof(glm::quat, w), refl_type(float) });

		return type;
	}

	RESOLVE_TYPE(glm::quat, init_quat)

		Struct init_ivec2() {
		Struct type("glm::ivec2", sizeof(glm::ivec2));
		type.fields.append({ "x", offsetof(glm::ivec2, x), refl_type(int) });
		type.fields.append({ "y", offsetof(glm::ivec2, y), refl_type(int) });

		return type;
	}

	RESOLVE_TYPE(glm::ivec2, init_ivec2)

	Struct init_mat4() {
		Struct type("glm::mat4", sizeof(glm::mat4));
		return type;
	}

	RESOLVE_TYPE(glm::mat4, init_mat4)

	Type init_sstring() {
		return Type{Type::SString, sizeof(sstring)};
	}

	RESOLVE_TYPE(sstring, init_sstring)

	Type init_string_buffer() {
		return Type{ Type::StringBuffer, sizeof(string_buffer) };
	}

	RESOLVE_TYPE(string_buffer, init_string_buffer)
}