//
//  reflection.h
//  PixC
//
//  Created by Lucas Goetz on 07/11/2019.
//  Copyright © 2019 Lucas Goetz. All rights reserved.
//

#pragma once

#include "core/container/string_view.h"
#include "core/container/vector.h"
#include <glm/glm.hpp>

using string = string_view;

namespace pixc {
    namespace reflection {
        struct Type {
            enum RefType {UInt, Int,Bool,Float,Char,Struct,Array,StaticArray,HybridArray,String,Ptr} type;
        };
        
        struct Field {
            string name;
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
            string name;
            vector<Field> fields;
            vector<Type*> template_args;
        };
        
        struct Namespace {
            string name;
            vector<Namespace*> namespaces;
            vector<StructType*> structs;
        };
        
        template<typename T>
        struct _type {};
        
#define GET_TYPE(type) get_type(pixc::reflection::_type<type>{})

        Type* get_type(_type<unsigned int>);
        Type* get_type(_type<string>);
        Type* get_type(_type<bool>);
        Type* get_type(_type<float>);
        Type* get_type(_type<int>);
        Type* get_type(_type<glm::vec2>);
        Type* get_type(_type<glm::vec3>);
        Type* get_type(_type<glm::vec4>);
        Type* get_type(_type<glm::ivec2>);
        
    };
}
