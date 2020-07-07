#pragma once

#include "core/container/string_view.h"
#include "core/container/vector.h"
#include "core/container/tvector.h"
#include "core/container/array.h"
#include "core/io/logger.h"
#include <glm/glm.hpp>

namespace refl {
	const uint REFLECT_TAG = 1 << 0;
	const uint SERIALIZE_TAG = 1 << 1;
	const uint PRINTABLE_TAG = 1 << 2;
	const uint COMPONENT_TAG = (1 << 3) | REFLECT_TAG | SERIALIZE_TAG;
	const uint HIDE_IN_EDITOR_TAG = 1 << 4;
	const uint LAYERMASK_TAG = 1 << 5;

	struct Type {
		enum RefType { UInt, Int, Bool, Float, Char, Struct, Union, Enum, Array, StaticArray, HybridArray, StringView, StringBuffer, SString, Ptr } type;
		uint size;
		string_view name;
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

		inline Array(string_view name, ArrayType type, Type* element, uint size, uint num_elements = 0) 
			: Type{ Type::Array, size, name }, arr_type(type), element(element), num(num) {}
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

	template<typename T>
	struct TypeResolver {
		static Type* get();
	};

#define refl_type(T) refl::TypeResolver< T >::get()


	template<typename T>
	struct TypeResolver<vector<T>> {
		static Type* get() {
			static string_buffer name = format("vector<", refl_type(T)->name, ">");
			static Array type(name, Array::Vector, refl_type(T), sizeof(vector<T>));
			return &type;
		}
	};

	template<int N, typename T>
	struct TypeResolver<T[N]> {
		static Type* get() {
			static string_buffer name = format(refl_type(T)->name, "[", N, "]");
			static Array type(name, Array::CArray, refl_type(T), sizeof(T[N]));
			return &type;
		}
	};

	template<typename T>
	struct TypeResolver<tvector<T>> {
		static Type* get() {
			static string_buffer name = format("tvector<", refl_type(T)->name, ">");
			static Array type(name, Array::TVector, refl_type(T), sizeof(tvector<T>));
			return &type;
		}
	};

	template<int N, typename T>
	struct TypeResolver<array<N, T>> {
		static Type* get() {
			static string_buffer name = format("array<", refl_type(T)->name, ">");
			static Array type(name, Array::StaticArray, refl_type(T), sizeof(array<N, T>), N);
			return &type;
		}
	};

	//template<typename T>
	//Type* get_type();
}