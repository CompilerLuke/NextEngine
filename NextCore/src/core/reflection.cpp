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

namespace refl {
	LinearAllocator reflection_allocator(mb(10));


#define RESOLVE_PRIMITIVE_TYPE(typ, enum_name) template<> struct TypeResolver<typ> { \
	static Type* get() {\
		static Type type{Type::enum_name, sizeof(type)};\
		return &type;\
	} \
};

#define RESOLVE_TYPE(typ, func) template<> struct TypeResolver<typ> { \
	static Type* get() { \
		static Struct type = func(); \
		return &type; \
	} \
};

	RESOLVE_PRIMITIVE_TYPE(uint, UInt)
	RESOLVE_PRIMITIVE_TYPE(u64, UInt)
	RESOLVE_PRIMITIVE_TYPE(float, Float)
	RESOLVE_PRIMITIVE_TYPE(int, Int)
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

	RESOLVE_TYPE(glm::vec4, init_vec4)

	Struct init_ivec2() {
		Struct type("glm::ivec2", sizeof(glm::ivec2));
		type.fields.append({ "x", offsetof(glm::ivec2, x), refl_type(int) });
		type.fields.append({ "y", offsetof(glm::ivec2, y), refl_type(int) });

		return type;
	}

	RESOLVE_TYPE(glm::ivec2, init_ivec2)

	template<typename T>
	struct TypeResolver<vector<T>> {
		static Type* get() {
			static Array type(Type::Vector, refl_type(T), sizeof(vector<T>));
			return &type;
		}
	};

	template<typename T>
	struct TypeResolver<tvector<T>> {
		static Type* get() {
			static Array type(Array::TVector, refl_type(T), sizeof(tvector<T>));
			return &type;
		}
	};

	template<int N, typename T>
	struct TypeResolver<array<N, T>> {
		static Type* get() {
			static Array type(Array::StaticArray, refl_type(T), sizeof(array<N, T>), N);
			return &type;
		}
	};
}