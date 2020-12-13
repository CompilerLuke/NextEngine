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

	struct Type {
		enum RefType { UInt, Int, Bool, Float, Char, Struct, Union, Alias, Enum, Array, StringView, StringBuffer, SString, Ptr } type;
		uint size;
		sstring name;
	};

	struct Alias : Type {
		Type* aliasing;

		inline Alias(const char* name, Type* type) : Type{ Type::Alias, type->size, name }, aliasing(type) {}
	};

	struct Field {
		sstring name;
		uint offset;
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

    enum DiffType {
        UNCHANGED_DIFF, ADDED_DIFF, REMOVED_DIFF, EDITED_DIFF
    };

    struct DiffField;

    struct DiffOfType {
        DiffType type;
        string_view previous_name;
        string_view current_name;
        u64 previous_size;
        u64 current_size;
        Type::RefType previous_type;
        Type::RefType current_type;
        vector<DiffField> fields;
    };

    struct DiffField {
        DiffType type;
        string_view previous_name;
        string_view current_name;
        uint previous_offset;
        uint current_offset;
        uint previous_order;
        uint current_order;
        Type* previous_type;
        Type* current_type;
        DiffOfType diff;
    };

    CORE_API DiffOfType diff_type(Type* a, Type* b) ;
}

//todo some of these should return Struct
CORE_API refl::Type* get_char_type();
CORE_API refl::Type* get_bool_type();
CORE_API refl::Type* get_uint_type();
CORE_API refl::Type* get_int_type();
CORE_API refl::Type* get_u64_type();
CORE_API refl::Type* get_i64_type();
CORE_API refl::Type* get_float_type();
CORE_API refl::Type* get_sstring_type();
CORE_API refl::Type* get_string_view_type();
CORE_API refl::Type* get_string_buffer_type();
CORE_API refl::Type* get_vec2_type();
CORE_API refl::Type* get_vec3_type();
CORE_API refl::Type* get_vec4_type();
CORE_API refl::Type* get_quat_type();
CORE_API refl::Type* get_mat4_type();

CORE_API refl::Array* make_vector_type(refl::Type* type);
CORE_API refl::Array* make_tvector_type(refl::Type* type);
CORE_API refl::Array* make_array_type(uint N, uint size, refl::Type* type);
CORE_API refl::Array* make_carray_type(uint N, refl::Type* type);
