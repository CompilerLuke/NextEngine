#pragma once

#include "core/container/string_view.h"
#include "core/container/vector.h"
#include <glm/glm.hpp>

namespace refl {
	struct Type {
		enum RefType { UInt, Int, Bool, Float, Char, Struct, Array, StaticArray, HybridArray, StringView, StringBuffer, SString, Ptr } type;
		uint size;
	};

	struct Field {
		string_view name;
		int offset;
		Type* type;
	};

	struct Ptr : Type {
		Type* element;
		bool ref;
	};

	struct Array : Type {
		enum ArrayType { Vector, TVector, StaticArray } arr_type;

		Type* element;
		int num;

		int length_offset;

		Array(ArrayType, Type* element, uint size, uint num_elements = 0);
	};

	struct Struct : Type {
		string_view name;
		vector<Field> fields;
		vector<Type*> template_args;
		
		Struct(string_view name, uint size);
	};

	struct Namespace {
		string_view name;
		vector<Namespace*> namespaces;
		vector<Struct*> structs;
	};

	template<typename T>
	struct TypeResolver {
		static Type* get();
	};

#define refl_type(T) refl::TypeResolver<T>::get()

	//template<typename T>
	//Type* get_type();
}