#pragma once

#include "core/container/string_view.h"
#include "core/container/vector.h"
#include "core/container/tvector.h"
#include "core/container/array.h"
#include "core/io/logger.h"
#include <glm/glm.hpp>

//todo all allocations for reflection objects should use one clearable allocator per dll
namespace refl {
	const uint REFLECT_TAG = 1 << 0;
	const uint SERIALIZE_TAG = 1 << 1;
	const uint PRINTABLE_TAG = 1 << 2;
	const uint COMPONENT_TAG = (1 << 3) | REFLECT_TAG | SERIALIZE_TAG;
	const uint HIDE_IN_EDITOR_TAG = 1 << 4;
	const uint LAYERMASK_TAG = 1 << 5;

	struct Type {
		enum RefType { UInt, Int, Bool, Float, Char, Struct, Union, Alias, Enum, Array, StringView, StringBuffer, SString, Ptr } type;
		uint size;
		string_view name;
	};

	struct Alias : Type {
		Type* aliasing;

		inline Alias(const char* name, Type* type) : Type{ Type::Alias, type->size, name }, aliasing(type) {}
	};

	struct Field {
		string_view name;
		int offset;
		Type* type;
		uint flags;
	};

	struct Ptr : Type {
		Type* element;
		bool ref;
	};

	struct Array : Type {
		enum ArrayType { Vector, TVector, CArray, StaticArray } arr_type;

		Type* element;
		int num;

		int length_offset;

		inline Array(ArrayType arr_type, uint size, const char* name, Type* element, uint num = 0)
			: Type{ Type::Array, size, name }, element(element), arr_type(arr_type), num(num) {}
	};

	struct Enum : Type {
		struct Value {
			string_view name;
			int value;
		};

		vector<Value> values;

		inline Enum(string_view name, uint size) : Type{ Type::Enum, size, name } {}
	};

	/* technically a tagged union, since regular unions cannot be serialized!*/
	struct Union : Type {
		vector<Field> fields;
		vector<Type> types;
		
		int type_offset;
	};

	struct Struct : Type {
		vector<Field> fields;
		vector<Type*> template_args;

		inline Struct(string_view name, uint size) : Type{Type::Struct, size, name} {}
	};

	struct Namespace {
		vector<Namespace*> namespaces;
		vector<Struct*> structs;
	};
}

//todo some of these should return Struct
ENGINE_API refl::Type* get_char_type();
ENGINE_API refl::Type* get_bool_type();
ENGINE_API refl::Type* get_uint_type();
ENGINE_API refl::Type* get_int_type();
ENGINE_API refl::Type* get_u64_type();
ENGINE_API refl::Type* get_i64_type();
ENGINE_API refl::Type* get_float_type();
ENGINE_API refl::Type* get_sstring_type();
ENGINE_API refl::Type* get_string_view_type();
ENGINE_API refl::Type* get_string_buffer_type();
ENGINE_API refl::Type* get_vec2_type();
ENGINE_API refl::Type* get_vec3_type();
ENGINE_API refl::Type* get_vec4_type();
ENGINE_API refl::Type* get_quat_type();
ENGINE_API refl::Type* get_mat4_type();

ENGINE_API refl::Array* make_vector_type(refl::Type* type);
ENGINE_API refl::Array* make_tvector_type(refl::Type* type);
ENGINE_API refl::Array* make_array_type(uint N, refl::Type* type);
ENGINE_API refl::Array* make_carray_type(uint N, refl::Type* type);