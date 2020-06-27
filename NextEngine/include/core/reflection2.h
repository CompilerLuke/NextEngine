#pragma once

#include "core/container/string_view.h"
#include "core/container/vector.h"
#include <glm/glm.hpp>

struct Type {
	enum RefType { UInt, Int, Bool, Float, Char, Struct, Array, StaticArray, HybridArray, String, Ptr } type;
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
	enum ArrayType { Static, Hybrid, Dynamic } arr_type;

	Type* element;
	int num;

	int length_offset;
};

struct StructType : Type {
	string_view name;
	vector<Field> fields;
	vector<Type*> template_args;
};

struct Namespace {
	string_view name;
	vector<Namespace*> namespaces;
	vector<StructType*> structs;
};

template<typename T>
Type* get_type();
