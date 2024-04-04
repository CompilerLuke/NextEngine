//#include "stdafx.h"
#include "core/memory/linear_allocator.h"
#include "core/reflection.h"
#include "core/container/vector.h"
#include "core/container/tvector.h"
#include "core/container/array.h"
#include "core/container/sstring.h"
#include "core/container/string_buffer.h"
#include "core/container/hash_map.h"
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

RESOLVE_TYPE(glm::vec4, vec4)

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
	snprintf(name, 100, "vector<%s>", type->name.c_str());

	return PERMANENT_ALLOC(Array, Array::Vector, sizeof(vector<char>), name, type);
}

Array* make_tvector_type(Type* type) {
	char* name = PERMANENT_ARRAY(char, 100);
	snprintf(name, 100, "tvector<%s>", type->name.c_str());

	return PERMANENT_ALLOC(Array, Array::TVector, sizeof(tvector<char>), name, type);
}

Array* make_array_type(uint N, uint size, Type* type) {
	char* name = PERMANENT_ARRAY(char, 100);
	snprintf(name, 100, "array<%i, %s>", N, type->name.c_str());

	return PERMANENT_ALLOC(Array, Array::StaticArray, size, name, type);
}

Array* make_carray_type(uint N, Type* type) {
	char* name = PERMANENT_ARRAY(char, 100);
	snprintf(name, 100, "%s[%i]", type->name.c_str(), N);

	return PERMANENT_ALLOC(Array, Array::CArray, sizeof(void*) + type->size * N, name, type);
}



//DIFF
namespace refl {

DiffOfType diff_type(Type* a, Type* b);

DiffField diff_field(Field& a, Field& b) {
    DiffField field = {};
    field.previous_name = a.name;
    field.current_name = b.name;
    field.previous_offset = a.offset;
    field.current_offset = b.offset;
    field.previous_type = a.type;
    field.current_type = b.type;
    
    if (field.previous_type->name == field.current_type->name
    ) {
        field.type = UNCHANGED_DIFF;
    } else {
        field.type = EDITED_DIFF;
    }
    
    field.diff = diff_type(a.type, b.type);
    //assert(field.diff.type != REMOVED_DIFF && field.diff.type != ADDED_DIFF);
    if (field.diff.type == EDITED_DIFF) field.type = EDITED_DIFF;
    
    return field;
}

Field* find_field(slice<Field> fields, string_view name) {
    for (uint i = 0; i < fields.length; i++) {
        if (fields[i].name == name) return &fields[i];
    }
    return nullptr;
}

DiffOfType diff_type(Type* a, Type* b) {
    DiffOfType diff = {};
    if (a) {
        diff.previous_name = a->name;
        diff.previous_type = a->type;
        
        if (!b) {
            diff.type = REMOVED_DIFF;
            return diff;
        }
    }
    if (b) {
        diff.current_name = b->name;
        diff.current_type = b->type;
        
        if (!a) {
            diff.type = ADDED_DIFF;
            return diff;
        }
    }
    
    diff.previous_size = a->size;
    diff.current_size = b->size;
    
    if (a->type == Type::Struct && b->type == Type::Struct) {
        Struct* a_struct = (Struct*)a;
        Struct* b_struct = (Struct*)b;
        
        hash_map<sstring, Field*, 103> a_fields;
        hash_map<sstring, Field*, 103> b_fields;
        
        diff.fields.reserve(a_struct->fields.length);
        
        bool edited = false;
        
        for (uint i = 0; i < a_struct->fields.length; i++) {
            Field* field_a = &a_struct->fields[i];
            Field* field_b = find_field(b_struct->fields, field_a->name);
            
            if (!field_b) {
                DiffField field = {};
                field.previous_name = field_a->name;
                field.previous_offset = field_a->offset;
                field.previous_type = field_a->type;
                field.type = REMOVED_DIFF;
                edited = true;
                
                diff.fields.append(field);
            } else {
                DiffField field = diff_field(*field_a, *field_b);
                edited |= field.type == EDITED_DIFF;
                field.previous_order = field_a - a_struct->fields.data;
                field.current_order = field_b - b_struct->fields.data;
                diff.fields.append(field);
            }
        }
        
        for (int i = 0; i < b_struct->fields.length; i++) {
            Field* field_b = &b_struct->fields[i];
            Field* field_a = find_field(a_struct->fields, field_b->name);
            
            if (!field_a) {
                DiffField field = {};
                field.current_name = field_b->name;
                field.current_offset = field_b->offset;
                field.current_type = field_b->type;
                field.type = ADDED_DIFF;
                edited = true;
                
                diff.fields.append(field);
            } 
        }
        
        if (edited) {
            diff.type = EDITED_DIFF;
        }
    }
    
    return diff;
}

}
