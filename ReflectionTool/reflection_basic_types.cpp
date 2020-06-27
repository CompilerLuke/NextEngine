//
//  reflection_basic_types.cpp
//  PixC
//
//  Created by Lucas Goetz on 08/11/2019.
//  Copyright © 2019 Lucas Goetz. All rights reserved.
//

#include "helper.h"
#include "core/memory/linear_allocator.h"
#include "reflection.h"

namespace pixc {
    namespace reflection {
        LinearAllocator reflection_allocator(mb(10));
        
        Type* make_Type(Type::RefType ref_type) {
            Type* type = alloc<Type>(&reflection_allocator);
            type->type = ref_type;
            return type;
        }
        
        StructType* make_StructType() {
            StructType* type = alloc<StructType>(&reflection_allocator);
            type->type = Type::Struct;
            type->fields.allocator = &reflection_allocator;
            return type;
        }
        
#define RESOLVE_TYPE(typ, enum_name) Type* get_type(_type<typ>) {\
        static Type* type = make_Type(Type::enum_name);\
        return type;\
    }\

        RESOLVE_TYPE(unsigned int, UInt)
        RESOLVE_TYPE(float, Float)
        RESOLVE_TYPE(int, Int)
        RESOLVE_TYPE(bool, Bool)
        RESOLVE_TYPE(string, String)
        
        StructType* init_vec2() {
            StructType* type = make_StructType();
            type->name = "glm::vec2";
            type->fields.append({"x", offsetof(glm::vec2, x), GET_TYPE(float) });
            type->fields.append({"y", offsetof(glm::vec2, y), GET_TYPE(float) });
            
            return type;
        }
        
        StructType* init_vec3() {
            StructType* type = make_StructType();
            type->name = "glm::vec3";
            type->fields.append({"x", offsetof(glm::vec3, x), GET_TYPE(float) });
            type->fields.append({"y", offsetof(glm::vec3, y), GET_TYPE(float) });
            type->fields.append({"z", offsetof(glm::vec3, z), GET_TYPE(float) });
            
            return type;
        }
        
        Type* get_type(_type<glm::vec2>) {
            static StructType* type = init_vec2();
            return type;
        }
        
        Type* get_type(_type<glm::vec3>) {
            static StructType* type = init_vec3();
            return type;
        }
        
        StructType* init_vec4() {
            StructType* type = make_StructType();
            type->name = "glm::vec4";
            type->fields.append({"x", offsetof(glm::vec4, x), GET_TYPE(float) });
            type->fields.append({"y", offsetof(glm::vec4, y), GET_TYPE(float) });
            type->fields.append({"z", offsetof(glm::vec4, z), GET_TYPE(float) });
            type->fields.append({"w", offsetof(glm::vec4, w), GET_TYPE(float) });
            
            return type;
        }
        
        Type* get_type(_type<glm::vec4>) {
            static StructType* type = init_vec4();
            return type;
        }
        
        StructType* init_ivec2() {
            StructType* type = make_StructType();
            type->name = "glm::ivec2";
            type->fields.append({"x", offsetof(glm::ivec2, x), GET_TYPE(int) });
            type->fields.append({"y", offsetof(glm::ivec2, y), GET_TYPE(int) });
            
            return type;
        }

        Type* get_type(_type<glm::ivec2>) {
            static StructType* type = init_vec2();
            return type;
        }
        
        
    }
}
