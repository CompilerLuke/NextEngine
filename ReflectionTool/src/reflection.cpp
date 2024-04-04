//
//  reflection.cpp
//  PixC
//
//  Created by Lucas Goetz on 29/10/2019.
//  Copyright © 2019 Lucas Goetz. All rights reserved.
//

#include "core/container/tvector.h"
#include "core/container/string_view.h"
#include "core/container/hash_map.h"
#include "core/container/array.h"
#include "core/memory/allocator.h"
#include "core/memory/linear_allocator.h"
#include "lexer.h"
#include "error.h"
#include "helper.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include "core/serializer.h"

#ifdef __APPLE__
#define _stat stat
#endif

using string = string_view;

const uint MAX_FILEPATH = 200;

//todo eliminate namespaces!
namespace pixc::reflection::compiler {
    struct Type {
        enum Kind {Int,Uint,I64,U64,Bool,Float,Double,Char,StringView,SString,StringBuffer,Struct,Enum,Union,StructRef,Array,Alias,Ptr,IntArg} type;
    };

    struct Field {
        const char* name;
        Type* type;
        uint flags;
    };

    struct Name {
        const char* iden = "";
        const char* type = "";
        const char* full = "";
    };

    struct StructRef : Type {
        Name name;
        tvector<Type*> template_args;
    };

    struct Ptr : Type {
        Type* element;
        bool ref;
    };
    
    struct IntArg : Type {
        int value;
    };

    struct Array : Type {
        enum ArrayType { Vector, TVector, Slice, CArray, StaticArray } arr_type;
        Type* element;
        uint num;
    };

    const uint REFLECT_TAG = 1 << 0;
    const uint SERIALIZE_TAG = 1 << 1;
    const uint PRINTABLE_TAG = 1 << 2;
    const uint COMPONENT_TAG = (1 << 3) | REFLECT_TAG | SERIALIZE_TAG;
    const uint ENTITY_FLAG_TAG = 1 << 4;
    const uint TAGGED_UNION_TAG = 1 << 5;
    const uint TRIVIAL_TAG = 1 << 6;
    const uint NON_TRIVIAL_TAG = 1 << 7;
    const uint SYSTEM_COMPONENT_TAG = (1 << 8) | COMPONENT_TAG;

    struct Constant {
        const char* name;
        int value;
    };

    struct AliasType : Type {
        Name name;
        Type* aliasing;
        uint flags = 0;
    };

    struct EnumType : Type {
        Name name;
        tvector<Constant> values;
        bool is_class = false;
        uint flags = 0; //todo make is class a flag
    };

    struct StructType : Type {
        Name name;
        tvector<Field> fields;
        uint flags = 0;
        tvector<Type*> template_args;
    };

    struct UnionType : Type {
        Name name;
        tvector<Field> fields;
        uint flags = 0;
    };

    struct Namespace {
        string name;
        tvector<Namespace*> namespaces;
        tvector<StructType*> structs;
        tvector<EnumType*> enums;
        tvector<AliasType*> alias;
        tvector<UnionType*> unions;
    };

    const uint MAX_COMPONENTS = 64;

    struct Reflector {
        error::Error* err;
        slice<lexer::Token> tokens;
        int i = 0;
        tvector<Namespace*> namespaces;
        tvector<const char*> header_files;
        const char* h_output;
        const char* output;
        const char* linking;
        const char* base;
        const char* include;
        const char* component;
        int component_id;
        LinearAllocator* allocator;
        hash_map<sstring, uint, MAX_COMPONENTS> name_to_id;
    };

    void raise_error(Reflector& ref, error::ErrorID type, string mesg, lexer::Token token) {
        ref.err->column = token.column;
        ref.err->id = type;
        ref.err->line = token.line;
        ref.err->mesg = mesg;
        ref.err->token_length = token.length_of_token;

        error::log_error(ref.err);

        exit(1);
    }

    void unexpected(Reflector& ref, lexer::Token token) {
        lexer::print_token(token);
        raise_error(ref, error::SyntaxError, "Came across unexpected token whilst parsing syntax!", token);
    };

    const char* as_null_terminated(Reflector& ref, string value) {
        char* buffer = alloc<char>(ref.allocator, value.length + 1);
        to_cstr(value, buffer, value.length + 1);

        return buffer;
    }

    const char* parse_name(Reflector& ref) {
        lexer::Token tok = ref.tokens[++ref.i];
        if (tok.type != lexer::Identifier) unexpected(ref, tok);
        
        return as_null_terminated(ref, tok.value);
    }

    lexer::Token assert_next(Reflector& ref, lexer::TokenType type) {
        lexer::Token token = ref.tokens[++ref.i];
        
        if (token.type != type) {
            unexpected(ref, token);
        }

        return token;
    }

    Type int_type = {Type::Int};
    Type uint_type = {Type::Uint};
    Type i64_type = {Type::I64};
    Type u64_type = {Type::U64};
    Type float_type = {Type::Float};
    Type double_type = {Type::Double};
    Type bool_type = {Type::Bool};
    Type char_type = {Type::Char};
    Type string_view_type = {Type::StringView};
    Type string_buffer_type = {Type::StringBuffer};
    Type sstring_type = {Type::SString};

    Type* parse_type(Reflector& ref);

    lexer::Token next_token(Reflector& ref) { return ref.tokens[++ref.i]; }
    lexer::Token curr_token(Reflector& ref) { return ref.tokens[ref.i]; }
    lexer::Token look_ahead(Reflector& ref) { return ref.tokens[ref.i + 1]; }

    void append_to_buffer(string& buffer, uint capacity, string str) {
        memcpy((char*)buffer.data + buffer.length, str.data, str.length);
        buffer.length += str.length;
    }

    int parse_int_expression(Reflector& ref) {
        lexer::Token token = curr_token(ref);
        if (token.type != lexer::Int) unexpected(ref, token);
        
        char buffer[100];
        to_cstr(token.value, buffer, 100);
        return atoi(buffer);
    }

    const char* make_namespaced_name(Reflector& ref, string name, string divider) {
        char* buffer = alloc<char>(ref.allocator, 100);
        string str = { buffer,0 };

        for (uint i = 1; i < ref.namespaces.length; i++) {
            append_to_buffer(str, 100, ref.namespaces[i]->name);
            append_to_buffer(str, 100, divider);
        }

        append_to_buffer(str, 100, name);
        buffer[str.length] = '\0';

        return buffer;
    }

    Name make_Name(Reflector& ref, const char* name) {
        Name result;
        result.iden = name;
        result.type = make_namespaced_name(ref, name, "::");
        result.full = make_namespaced_name(ref, name, "_");
        return result;
    }


    Type* parse_type_inner(Reflector& ref) {
        lexer::Token tok = ref.tokens[ref.i];
        if (tok.type == lexer::IntType) return &int_type;
        if (tok.type == lexer::UintType) return &uint_type;
        if (tok.type == lexer::I64Type) return &i64_type;
        if (tok.type == lexer::U64Type) return &u64_type;
        if (tok.type == lexer::FloatType) return &float_type;
        if (tok.type == lexer::BoolType) return &bool_type;
        if (tok.type == lexer::CharType) return &char_type;
        //if (tok.type == lexer::SStringType) return &sstring_type;
        //if (tok.type == lexer::StringBufferType) return &string_buffer_type;
        //if (tok.type == lexer::StringViewType) return &string_view_type;

        if (tok.type == lexer::Identifier) {
            int name_length = 0;
            char* buffer = alloc<char>(ref.allocator, 100);
            
            string type_name{ buffer,0 };

            append_to_buffer(type_name, 100, tok.value);
            
            while (ref.tokens[ref.i + 1].type == lexer::DoubleColon) {
                ref.i += 2;
                tok = ref.tokens[ref.i];

                append_to_buffer(type_name, 100, "::");
                append_to_buffer(type_name, 100, tok.value);
            };

            buffer[type_name.length] = '\0';

            const char* struct_name = as_null_terminated(ref, tok.value);

            if (type_name == "vector") {
                Array* array_type = alloc<Array>(ref.allocator);
                array_type->type = Type::Array;
                array_type->arr_type = Array::Vector;
                array_type->num = 0;
                
                assert_next(ref, lexer::LessThan);
                next_token(ref);

                array_type->element = parse_type(ref);
                assert_next(ref, lexer::GreaterThan);

                return array_type;
            }
            else if (type_name == "slice") {
                Array* array_type = alloc<Array>(ref.allocator);
                array_type->type = Type::Array;
                array_type->arr_type = Array::Slice;
                array_type->num = 0;

                assert_next(ref, lexer::LessThan);
                next_token(ref);

                array_type->element = parse_type(ref);
                assert_next(ref, lexer::GreaterThan);

                return array_type;
            }
            else if (type_name == "array") {
                Array* array_type = alloc<Array>(ref.allocator);
                array_type->type = Type::Array;
                array_type->arr_type = Array::StaticArray;

                assert_next(ref, lexer::LessThan);
                next_token(ref);

                array_type->num = parse_int_expression(ref);
                
                assert_next(ref, lexer::Comma);
                next_token(ref);

                array_type->element = parse_type(ref);

                assert_next(ref, lexer::GreaterThan);

                return array_type;
            }
            else {
                StructRef* struct_ref = alloc<StructRef>(ref.allocator);
                struct_ref->type = Type::StructRef;
                struct_ref->name.iden = struct_name; //  make_Name(ref, struct_name);
                struct_ref->name.full = struct_name;
                struct_ref->name.type = type_name.data;

                return struct_ref;
            }

            /*
            if (ref.tokens[ref.i + 1].type == lexer::LessThan) {
                ref.i++;
                while (true) {
                    ref.i++;
                    
                    if (ref.tokens[ref.i].type == lexer::Int) {
                        char buffer[100];
                        to_cstr(ref.tokens[ref.i].value, buffer, 100);
                        
                        IntArg* arg = alloc<IntArg>(ref.allocator);
                        arg->type = Type::IntArg;
                        arg->value = atoi(buffer);
                        
                        struct_ref->template_args.append((Type*)arg);
                    } else {
                        struct_ref->template_args.append(parse_type(ref));
                    }
                    
                    if (ref.tokens[++ref.i].type != lexer::Comma) {
                        if (ref.tokens[ref.i].type == lexer::GreaterThan) break;
                        else unexpected(ref, ref.tokens[ref.i]);
                    }
                }
            }*/
            
        };
        
        unexpected(ref, tok);
        
        return {};
    }

    Type* parse_type(Reflector& ref) {
        Type* type = parse_type_inner(ref);
        if (ref.tokens[ref.i + 1].type == lexer::MulOp) {
            Ptr* ptr = alloc<Ptr>(ref.allocator);
            ptr->element = type;
            ref.i++;
            return ptr;
        }
        return type;
    }

    AliasType* make_AliasType(Reflector& ref, const char* name, Type* type, uint flags) {
        AliasType* alias_type = alloc<AliasType>(ref.allocator);
        alias_type->type = Type::Alias;
        alias_type->name = make_Name(ref, name);
        alias_type->aliasing = type;
        alias_type->flags = flags;
        ref.namespaces.last()->alias.append(alias_type);
        return alias_type;
    }

    StructType* make_StructType(Reflector& ref, const char* name) {
        StructType* struct_type = alloc<StructType>(ref.allocator);
        struct_type->type = Type::Struct;
        struct_type->name = make_Name(ref, name);
        ref.namespaces.last()->structs.append(struct_type);
        return struct_type;
    }

    EnumType* make_EnumType(Reflector& ref, const char* name, bool is_class) {
        EnumType* enum_type = alloc<EnumType>(ref.allocator);
        enum_type->type = Type::Enum;
        enum_type->name = make_Name(ref, name);
        enum_type->is_class = is_class;
        ref.namespaces.last()->enums.append(enum_type);
        return enum_type;
    }

    UnionType* make_UnionType(Reflector& ref, const char* name, uint flags) {
        UnionType* union_type = alloc<UnionType>(ref.allocator);
        union_type->type = Type::Union;
        union_type->name = make_Name(ref, name);
        union_type->flags = flags;
        ref.namespaces.last()->unions.append(union_type);
        return union_type;
    }

    Namespace* make_Namespace(Reflector& ref, string name) {
        Namespace* space = alloc<Namespace>(ref.allocator);
        space->name = as_null_terminated(ref, name);
        return space;
    }

    Namespace* push_Namespace(Reflector& ref, string name) {
        Namespace* space = make_Namespace(ref, name);
        
        ref.namespaces.last()->namespaces.append(space);
        ref.namespaces.append(space);
        return space;
    }

    void pop_Namespace(Reflector& ref) {
        Namespace* space = ref.namespaces.pop();
    }


    const uint IN_STRUCT = 1 << 3;

    EnumType* parse_enum(Reflector& ref, uint flags) {
        bool is_enum_class = false;
        if (look_ahead(ref).type == lexer::Class) {
            ref.i++;
            is_enum_class = true;
        }

        const char* name = parse_name(ref);
        EnumType* enum_type = make_EnumType(ref, name, is_enum_class);

        assert_next(ref, lexer::Open_Bracket);
    
        ref.i++;

        uint last_enum_value = 0;

        for (; ref.tokens[ref.i].type != lexer::Close_Bracket && ref.i < ref.tokens.length; ref.i++) {
            lexer::Token token = curr_token(ref);
            if (token.type != lexer::Identifier) {
                unexpected(ref, token);
                return nullptr;
            }

            Constant constant = {};
            constant.name = as_null_terminated(ref, token.value);

            printf("ENUM CONSTANT %s\n", constant.name);
            
            token = next_token(ref);
            if (token.type == lexer::AssignOp) {
                token = assert_next(ref, lexer::Int);
                constant.value = atoi(token.value.c_str());
            }
            else if (token.type == lexer::Comma) {
                constant.value = last_enum_value;
            }
            else if (token.type == lexer::Close_Bracket) {
                constant.value = last_enum_value;
                break;
            }
            else {
                unexpected(ref, token);
            }

            enum_type->values.append(constant);
            last_enum_value++;
        }

        lexer::Token token = next_token(ref);
        if (flags & IN_STRUCT && token.type == lexer::Identifier) {
            return enum_type;
        }
        else if (token.type != lexer::SemiColon) {
            unexpected(ref, token);
            return nullptr;
        }
    }

    void parse_alias(Reflector& ref, uint flags) {
        const char* name = parse_name(ref);
        assert_next(ref, lexer::AssignOp);
        next_token(ref);
        Type* aliasing = parse_type(ref);
        assert_next(ref, lexer::SemiColon);

        make_AliasType(ref, name, aliasing, flags);
    }

    UnionType* parse_union(Reflector& ref, uint flags) {
        const char* name = "";
        
        lexer::Token token = look_ahead(ref);
        if (token.type != lexer::Open_Bracket) {
            name = parse_name(ref);
            assert_next(ref, lexer::Open_Bracket);
        }
        else if (!(flags & IN_STRUCT)) {
            raise_error(ref, error::SyntaxError, "Expecting name", token);
            return nullptr;
        }
        else {
            ref.i++;
        }
        
        UnionType* union_type = make_UnionType(ref, name, flags);

        ref.i++;

        uint last_enum_value = 0;

        for (; ref.tokens[ref.i].type != lexer::Close_Bracket && ref.i < ref.tokens.length; ref.i++) {
            Field field = {};
            
            field.type = parse_type(ref);
            field.name = parse_name(ref);
        
            union_type->fields.append(field);

            assert_next(ref, lexer::SemiColon);
        }

        token = next_token(ref);
        if (flags & IN_STRUCT && token.type == lexer::Identifier) {
            return union_type;
        }
        else if (token.type != lexer::SemiColon) {
            unexpected(ref, token);
            return nullptr;
        }
        else {
            return strlen(name) == 0 ? union_type : nullptr;
        }
    }

    void parse_struct(Reflector& ref, uint flags) {
        const char* name = parse_name(ref);
        if (ref.tokens[ref.i + 1].type == lexer::SemiColon) {
            ref.i++;
            return; //forward declaration
        }
        
        assert_next(ref, lexer::Open_Bracket);
        
        StructType* struct_type = make_StructType(ref, name);
        struct_type->flags = flags;
        
        Namespace* space = push_Namespace(ref, name);
        
        ref.i++;

        for (; ref.tokens[ref.i].type != lexer::Close_Bracket && ref.i < ref.tokens.length; ref.i++) {
            lexer::Token token = ref.tokens[ref.i];
            Type* type = nullptr;

            
            bool is_constructor = token.value == name && ref.tokens[ref.i+1].type == lexer::Open_Paren;
            bool is_static = token.type == lexer::Static;
            bool is_refl_false = token.value == "REFL_FALSE";
            bool ignore = is_constructor || is_static || is_refl_false;

            if (ignore) {
                if (is_refl_false) struct_type->flags |= NON_TRIVIAL_TAG; //in some cases it may be the correct decision to not keep around the ignored fields in serialization data
                
                uint bracket_count = 0;
                while (ref.tokens[++ref.i].type != lexer::SemiColon || bracket_count > 0) {
                    if (ref.tokens[ref.i].type == lexer::Open_Bracket) bracket_count++;
                    else if (ref.tokens[ref.i].type == lexer::Close_Bracket) {
                        if (--bracket_count == 0) break;
                    }
                };
                continue;
            }
            else if (token.type == lexer::Struct) {
                parse_struct(ref, 0);
                continue;
            }
            else if (token.type == lexer::Enum) {
                type = parse_enum(ref, IN_STRUCT);
                ref.i--;

                if (!type) continue;
            }
            else if (token.type == lexer::Union) {
                UnionType* union_type = parse_union(ref, IN_STRUCT);
                if (!union_type) continue;
                else if (strlen(union_type->name.iden) == 0) {
                    struct_type->fields.append({"", union_type});
                    continue;
                }
                else {
                    ref.i--;
                    type = union_type;
                }
            }
            else {
                type = parse_type(ref);

                bool is_slice = type->type == Type::Array && ((Array*)type)->arr_type == Array::Slice;
                if (is_slice && (flags & SERIALIZE_TAG)) {
                    raise_error(ref, error::SemanticError, "Cannot generate serialization function for slice, as allocation and ownership is unclear!", token);
                }
            }
            
            const char* name = parse_name(ref);
                
            if (ref.tokens[ref.i + 1].type == lexer::Open_SquareBracket) {
                ref.i++;
                Array* arr = alloc<Array>(ref.allocator);
                arr->type = Type::Array;
                arr->arr_type = Array::CArray;
                arr->element = type;
                    
                assert_next(ref, lexer::Int);
                    
                char buffer[100];
                to_cstr(ref.tokens[ref.i].value, buffer, 100);
                arr->num = atoi(buffer);
                    
                assert_next(ref, lexer::Close_SquareBracket);
                    
                type = arr;
            }
                
            if (ref.tokens[ref.i + 1].type != lexer::AssignOp) {
                assert_next(ref, lexer::SemiColon);
            } else {
                while (ref.tokens[++ref.i].type != lexer::SemiColon) {};
            }
                
            struct_type->fields.append({name, type});
        }
        
        assert_next(ref, lexer::SemiColon);
        
        pop_Namespace(ref);
    }

    /*void parse_inner(Reflector& ref);

    void parse_namespace(Reflector& ref) {
        string name = parse_name(ref);
        assert_next(ref, lexer::Open_Bracket);
        
        Namespace* space = push_Namespace(ref, name);
        
        int bracket = 0;
        
        for (; ref.i < ref.tokens.length; ref.i++) {
            if (ref.tokens[ref.i].type == lexer::Open_Bracket)
                bracket++;
            if (ref.tokens[ref.i].type == lexer::Close_Bracket)
                bracket--;
            
            if (bracket <= 0) break;
            parse_inner(ref);
        }
        
        pop_Namespace(ref);
    }*/

    /*void parse_inner(Reflector& ref) {
        bool reflect_struct = false;
        unsigned int flags = 0;
        
        for (;ref.tokens[ref.i].type == lexer::Identifier; ref.i++) {
            string token = ref.tokens[ref.i].value;
            
            if (token == "COMPONENT") {
                flags |= COMPONENT_TAG;
                reflect_struct = true;
            }
            
            if (token == "REFLECT") reflect_struct = true;
        }

        if (reflect_struct) {
            if (ref.tokens[ref.i].type == lexer::Struct) {
                parse_struct(ref, flags);
            }
        } else {
            int found_bracket = 0;

            while (ref.i < ref.tokens.length) {
                lexer::Token& token = ref.tokens[ref.i];
                
                if (token.type == lexer::Open_Bracket) {
                    found_bracket++;
                }

                if (token.type == lexer::Pragma) return;
                if (found_bracket == 0 && ref.tokens[ref.i].type == lexer::SemiColon) return;
                if (ref.tokens[ref.i].type == lexer::Close_Bracket && --found_bracket <= 0) return;
                ref.i++;
            }
        }
        
        //if (ref.tokens[ref.i].type == lexer::Namespace) {
        //   parse_namespace(ref);
        //}
    }*/

    void parse_entity_flag(Reflector& ref) {
        assert_next(ref, lexer::Open_Paren);
        const char* name = parse_name(ref);

        StructType* type = make_StructType(ref, name);
        type->flags |= ENTITY_FLAG_TAG;


        assert_next(ref, lexer::Close_Paren);
    }

    void parse(Reflector& ref) {
        
        
        for (; ref.i < ref.tokens.length; ref.i++) {
            
            bool reflect_struct = false;
            unsigned int flags = 0;

            for (;ref.tokens[ref.i].type == lexer::Identifier; ref.i++) {
                string token = ref.tokens[ref.i].value;

                //todo make TAG lexer group!
                if (token == "COMP") {
                    flags |= COMPONENT_TAG;
                    reflect_struct = true;
                }
                if (token == "SYSTEM_COMP") {
                    flags |= SYSTEM_COMPONENT_TAG;
                    reflect_struct = true;
                }
                if (token == "REFL") reflect_struct = true;
                if (token == "ENTITY_FLAG" && ref.tokens[ref.i - 1].type != lexer::Define) parse_entity_flag(ref);
            }

            if (reflect_struct) {
                lexer::Token token = curr_token(ref);

                switch (token.type) {
                case lexer::Struct: parse_struct(ref, flags); break;
                case lexer::Enum: parse_enum(ref, flags); break;
                case lexer::Union: parse_union(ref, flags); break;
                case lexer::Using: parse_alias(ref, flags); break;
                default: unexpected(ref, token); break;
                }
            }
        }
    }
    
    const char* indent_buffer = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";

    //todo implement hash based lookup
    Type* find_type(Namespace& space, string_view full_name) {
        for (Namespace* child : space.namespaces) {
            Type* result = find_type(*child, full_name);
            if (result) return result;
        }
        
        for (StructType* type : space.structs) {
            if (full_name == type->name.full) return type;
        }

        for (EnumType* type : space.enums) {
            if (full_name == type->name.full) return type;
        }

        for (AliasType* type : space.alias) {
            if (full_name == type->name.full) return type;
        }
        
        return NULL;
    }
    
    Type* find_type(Reflector& reflector, string_view full_name) {
        return find_type(*reflector.namespaces[0], full_name);
    }

    void dump_type(FILE* f, Type* type) {
        char buffer[100];
        
        if (type->type == Type::Int) fprintf(f, "int");
        if (type->type == Type::Uint) fprintf(f, "uint");
        if (type->type == Type::U64) fprintf(f, "u64");
        if (type->type == Type::I64) fprintf(f, "i64");
        if (type->type == Type::Float) fprintf(f, "float");
        if (type->type == Type::Bool) fprintf(f, "bool");
        if (type->type == Type::Enum) {
            EnumType* enum_type = (EnumType*)type;
            to_cstr(enum_type->name.type, buffer, 100);
            fprintf(f, "%s", buffer);
        }
        if (type->type == Type::StructRef) {
            StructRef* ref = (StructRef*)type;

            fprintf(f, "struct %s", ref->name.type);
        
            //this can likely be removed, template structs are rare
            if (ref->template_args.length > 0) {
                fprintf(f, "<");
                for (int i = 0; i < ref->template_args.length; i++) {
                    dump_type(f, ref->template_args[i]);
                    if (i < ref->template_args.length - 1) fprintf(f, ", ");
               }
               fprintf(f, ">");
            }
        }
        if (type->type == Type::IntArg) {
            fprintf(f, "%i", ((IntArg*)type)->value);
        }
        if (type->type == Type::Array) {
            Array* array_type = (Array*)type;

            switch (array_type->arr_type) {
            case Array::StaticArray:
                fprintf(f, "array<%i,", array_type->num);
                dump_type(f, array_type->element);
                fprintf(f, ">");
                break;
            case Array::Slice:
                fprintf(f, "slice<");
                dump_type(f, array_type->element);
                fprintf(f, ">");
                break;
            case Array::Vector:
                fprintf(f, "vector<");
                dump_type(f, array_type->element);
                fprintf(f, ">");
                break;
            case Array::CArray:
                dump_type(f, array_type->element);
                fprintf(f, "[%i]", array_type->num);
                break;
            }
        }
    }


    //todo cache result
    bool is_trivially_copyable(Reflector& reflector, Type* type) {
        switch (type->type) {
        case Type::Union:
        case Type::Struct: {
            StructType* struct_type = (StructType*)type;
            if (struct_type->flags & TRIVIAL_TAG) return true;
            if (struct_type->flags & NON_TRIVIAL_TAG) return false;

            for (Field& field : struct_type->fields) {
                if (!is_trivially_copyable(reflector, field.type)) {
                    struct_type->flags |= NON_TRIVIAL_TAG;
                    return false;
                }
            }


            struct_type->flags |= TRIVIAL_TAG;

            return true;
        }
        case Type::Array: {
            Array* array_type = (Array*)type;

            switch (array_type->arr_type) {
            case Array::Vector: return false;
            case Array::TVector: return false; //a bit strange to serialize a tvector
            case Array::CArray:
            case Array::StaticArray: return is_trivially_copyable(reflector, array_type->element);
            }
        }
        case Type::Alias: return is_trivially_copyable(reflector, ((AliasType*)type)->aliasing);
        case Type::StructRef: {
            StructRef* struct_ref = (StructRef*)type;

            Type* found = find_type(reflector, struct_ref->name.full);
            if (!found) return true; //todo this is not necessarily always true
            else return is_trivially_copyable(reflector, found);
        }

        default: return true;
        }
    }

    void write_trivial_to_buffer(FILE* f, const char* type, const char* name) {
        fprintf(f, "    write_n_to_buffer(buffer, &%s, sizeof(%s));\n", name, type);
    }

    //todo optimization, if the struct is trivially copyable simply write_n_to_buffer
    void serialize_type(FILE* f, Type* type, const char* name) {
        //fprintf(f, "    write_to_buffer(buffer, %s);\n", name);

        switch (type->type) {
        case Type::Enum: write_trivial_to_buffer(f, "int", name); break;
        case Type::Int: write_trivial_to_buffer(f, "int", name); break;
        case Type::Uint:  write_trivial_to_buffer(f, "uint", name); break;
        case Type::I64:  write_trivial_to_buffer(f, "i64", name); break;
        case Type::U64:  write_trivial_to_buffer(f, "u64", name); break;
        case Type::Float: write_trivial_to_buffer(f, "float", name); break;
        case Type::Bool: write_trivial_to_buffer(f, "bool", name); break;
        case Type::Char: write_trivial_to_buffer(f, "char", name); break;
        case Type::Union:
        case Type::StructRef: {
            StructType* struct_type = (StructType*)type;
            //Type* type = find_type();
            
            fprintf(f, "    write_%s_to_buffer(buffer, %s);\n", struct_type->name.full, name);
            //dump_type(f, type);
            //fprintf(f, ">::serialize(buffer, %s);\n", name);
            break;
        }
        case Type::Array: {
            Array* array_type = (Array*)type;
            char write_to[100];
            char length_str[100];
            snprintf(write_to, 100, "%s[i]", name);

            switch (array_type->arr_type) {
            case Array::CArray:
                snprintf(length_str, 100, "%i", array_type->num);
                break;
            case Array::Slice:
            case Array::StaticArray:
            case Array::Vector:
                fprintf(f, "    write_uint_to_buffer(buffer, %s.length);\n", name);
                snprintf(length_str, 100, "%s.length", name);
            }

            fprintf(f, "	for (uint i = 0; i < %s; i++) {\n     ", length_str);
            serialize_type(f, array_type->element, write_to);
            fprintf(f, "    }\n");
        }
        }
    }
    
    void read_trivial_from_buffer(FILE* f, const char* type, const char* name) {
        fprintf(f, "    read_n_from_buffer(buffer, &%s, sizeof(%s));\n", name, type);
    }

    //todo optimization, if the struct is trivially copyable simply write_n_to_buffer
    void deserialize_type(FILE* f, Type* type, const char* name) {
        //fprintf(f, "    write_to_buffer(buffer, %s);\n", name);

        switch (type->type) {
        case Type::Enum: read_trivial_from_buffer(f, "int", name); break;
        case Type::Int: read_trivial_from_buffer(f, "int", name); break;
        case Type::Uint:  read_trivial_from_buffer(f, "uint", name); break;
        case Type::I64:  read_trivial_from_buffer(f, "i64", name); break;
        case Type::U64:  read_trivial_from_buffer(f, "u64", name); break;
        case Type::Float: read_trivial_from_buffer(f, "float", name); break;
        case Type::Bool: read_trivial_from_buffer(f, "bool", name); break;
        case Type::Char: read_trivial_from_buffer(f, "char", name); break;
        case Type::StructRef: {
            StructType* struct_type = (StructType*)type;
            //Type* type = find_type();

            fprintf(f, "    read_%s_from_buffer(buffer, %s);\n", struct_type->name.full, name);
            //dump_type(f, type);
            //fprintf(f, ">::serialize(buffer, %s);\n", name);
            break;
        }
        case Type::Array: {
            Array* array_type = (Array*)type;
            char write_to[100];
            char length_str[100];
            snprintf(write_to, 100, "%s[i]", name);

            switch (array_type->arr_type) {
            case Array::TVector:
                fprintf(stderr, "Cannot serialize tvector");
                break;
            case Array::CArray:
                snprintf(length_str, 100, "%i", array_type->num);
                break;
            case Array::Slice:
            case Array::StaticArray:
                    
            case Array::Vector:
                fprintf(f, "    %s.resize(read_uint_from_buffer(buffer));\n", name);
                snprintf(length_str, 100, "%s.length", name);
            }

            fprintf(f, "	for (uint i = 0; i < %s; i++) {\n     ", length_str);
            deserialize_type(f,  array_type->element, write_to);
            fprintf(f, "    }\n");
        }
        }
    }


    void dump_enum_reflector(FILE* f, FILE* h, EnumType* type, const char* linking) {
        Name& name = type->name;
        
        fprintf(f, "refl::Enum init_%s() {\n", type->name.full);
        fprintf(f, "	refl::Enum type(\"%s\", sizeof(%s));\n", type->name.iden, type->name.type);

        for (Constant& constant : type->values) {
            fprintf(f, "	type.values.append({ \"%s\", %s::%s });\n", constant.name, type->name.type, constant.name);
        }

        fprintf(f, "	return type;\n");
        fprintf(f, "}\n\n");

        if (type->is_class) {
            fprintf(f, "void write_%s_to_buffer(SerializerBuffer& buffer, %s data) {\n", name.full, name.type);
            fprintf(f, "    write_uint_to_buffer(buffer, (uint)data);\n");
            fprintf(f, "}\n\n");

            fprintf(f, "void read_%s_from_buffer(DeserializerBuffer& buffer, %s data) {\n", name.full, name.type);
            fprintf(f, "    read_uint_from_buffer(buffer, (uint)data);\n");
            fprintf(f, "}\n\n");
        }

        fprintf(f, "refl::Enum* get_%s_type() {\n", type->name.full);
        fprintf(f, "	static refl::Enum type = init_%s();\n", type->name.full);
        fprintf(f, "	return &type;\n");
        fprintf(f, "}\n");

        fprintf(h, "%s refl::Enum* get_%s_type();\n", linking, type->name.full);
    }

    Type* find_type() {
        return NULL;
    }

    void dump_type_ref(FILE* f, Type* type) {
        switch (type->type) {
        case Type::Bool: fprintf(f, "get_bool_type()"); break;
        case Type::Uint: fprintf(f, "get_uint_type()"); break;
        case Type::Int: fprintf(f, "get_int_type()"); break;
        case Type::U64: fprintf(f, "get_u64_type()"); break;
        case Type::I64: fprintf(f, "get_i64_type()"); break;
        case Type::Float: fprintf(f, "get_float_type()"); break;
        case Type::Array: {
            Array* array = (Array*)type;

            switch (array->arr_type) {
            case Array::Vector:  fprintf(f, "make_vector_type("); break;
            case Array::TVector: fprintf(f, "make_tvector_type("); break;
            case Array::StaticArray:
                fprintf(f, "make_array_type(%i, sizeof(array<%i, ", array->num, array->num);
                dump_type(f, array->element);
                fprintf(f, ">), ");
                break;


            case Array::CArray: fprintf(f, "make_carray_type(%i, ", array->num); break;
            }

            dump_type_ref(f, array->element);
            fprintf(f, ")");
            break;
        }

        case Type::StructRef: {
            StructRef* ref = (StructRef*)type;
            fprintf(f, "get_%s_type()", ref->name.full);
            break;
        }

        case Type::Enum: {
            EnumType* ref = (EnumType*)type;
            fprintf(f, "get_%s_type()", ref->name.full);
            break;
        }


        }
    }

    void serialize_tagged_union(FILE* f, const char* type_name, UnionType* type, EnumType* tag, const char* variable, void(*serialize_type)(FILE*, Type*, const char*)) {
        fprintf(f, "    switch (%stype) {\n", variable);

        for (int i = 0; i < min(type->fields.length, tag->values.length); i++) {
            Field& field = type->fields[i];
            Constant& constant = tag->values[i];

            fprintf(f, "    case %s::%s:\n", type_name, constant.name);

            char write_to[100];
            snprintf(write_to, 100, "%s%s", variable, field.name);
            serialize_type(f, field.type, write_to);

            fprintf(f, "    break;\n\n");
        }

        fprintf(f, "    }\n");
    }

    /*
    void dump_union_reflector(FILE* f, FILE* h, UnionType* type, string prefix, Reflector& reflector, int indent = 0) {
        Name& name = type->name;
        const char* linking = reflector.linking;

        fprintf(f, "refl::Union init_%s() {\n", name.full);
        fprintf(f, "	refl::Union type(\"%s\", sizeof(%s));\n", name.full, name.type);

        for (Field& field : type->fields) {
            fprintf(f, "	type.fields.append({");
            fprintf(f, "\"%s\", 0, ", field.name);
            dump_type_ref(f, field.type);

            fprintf(f, "});\n");
        }

        fprintf(f, "	return type;\n");
        fprintf(f, "}\n\n");

        bool trivial = is_trivially_copyable(reflector, type);
        EnumType* tag = type->tag;

        if (!tag) {
            fprintf(h, "%s void write_%s_to_buffer(struct SerializerBuffer& buffer, union %s& data, %s tag);\n", linking, name.full, name.type, type->tag->type);
            fprintf(f, "void write_%s_to_buffer(SerializerBuffer& buffer, %s& data, %s tag) {\n", name.full, name.type, type->tag->type);
        }
        else {
            fprintf(h, "%s void write_%s_to_buffer(struct SerializerBuffer& buffer, union %s& data);\n", linking, name.full, name.type);
            fprintf(f, "void write_%s_to_buffer(SerializerBuffer& buffer, %s& data) {\n", name.full, name.type);
        }

        if (trivial) {
            fprintf(f, "    write_n_to_buffer(buffer, &data, sizeof(%s));\n", name.type);
        }
        else if (!tag) {
            fprintf(stderr, "Non tagged union %s must be trivially copyable", name.iden);
            abort();
        }
        else {
            serialize_tagged_union(f, type, tag, serialize_type);
        }
        fprintf(f, "}\n\n");

        if (!tag) {
            fprintf(h, "%s void read_%s_from_buffer(struct SerializerBuffer& buffer, union %s& data, %s tag);\n", linking, name.full, name.type, type->tag->type);
            fprintf(f, "void read_%s_from_buffer(SerializerBuffer& buffer, %s& data, %s tag) {\n", name.full, name.type, type->tag->type);
        }
        else {
            fprintf(h, "%s void read_%s_from_buffer(struct SerializerBuffer& buffer, union %s& data);\n", linking, name.full, name.type);
            fprintf(f, "void read_%s_from_buffer(SerializerBuffer& buffer, %s& data) {\n", name.full, name.type);
        }

        if (trivial) {
            fprintf(f, "    read_n_to_buffer(buffer, &data, sizeof(%s));\n", name.type);
        }
        else {
            fprintf(f, "switch (tag) {");

            for (int i = 0; i < min(type->fields.length, tag->values.length); i++) {
                Field& field = type->fields[i];
                Constant& constant = tag->values[i];

                fprintf(f, "case %s:\n", constant.name);

                char write_to[100];
                sprintf_s(write_to, "data.%s", field.name);
                deserialize_type(f, field.type, write_to);
            }

            fprintf(f, "}");
        }
        fprintf(f, "}\n\n");


        fprintf(h, "%s refl::Union* get_%s_type();\n", linking, name.full);

        fprintf(f, "refl::Union* get_%s_type() {\n", name.full);
        fprintf(f, "	static refl::Union type = init_%s();\n", name.full);
        fprintf(f, "	return &type;\n");
        fprintf(f, "}\n\n");


    }*/


    void dump_struct_reflector(FILE* f, FILE* h, StructType* type, string prefix, Reflector& reflector, int indent = 0) {
        if (type->flags & ENTITY_FLAG_TAG) return;
        
        Name& name = type->name;
        const char* linking = reflector.linking;
        
        fprintf(f, "refl::Struct init_%s() {\n", name.full);
        fprintf(f, "	refl::Struct type(\"%s\", sizeof(%s));\n", name.full, name.type);

        EnumType* tag_type = nullptr;
        bool is_tagged_union = false;

        //Support tags
        for (Field& field : type->fields) {
            if (field.name == "" && field.type->type == Type::Union) {
                UnionType* union_type = (UnionType*)type;

                //todo generate correct reflection data
                fprintf(f, "    static refl::Union inline_union = { refl::Type::Union, 0, \"%s\" };\n", union_type->name.full); //no clue what the size
                //todo reflect fields
                fprintf(f, "    type.fields.append({ \"\", offsetof(%s, %s), &inline_union });\n", name.type, union_type->fields[0].name);
                is_tagged_union = true;
            }
            else if (strcmp(field.name, "type") == 0 && field.type->type == Type::Enum) {
                tag_type = (EnumType*)field.type;
            }
            else {
                fprintf(f, "	type.fields.append({");
                fprintf(f, "\"%s\", offsetof(%s, %s), ", field.name, name.type, field.name);
                dump_type_ref(f, field.type);

                fprintf(f, "});\n");
            }
        }

        fprintf(f, "	return type;\n");
        fprintf(f, "}\n\n");

        bool trivial = is_trivially_copyable(reflector, type);

        fprintf(h, "%s void write_%s_to_buffer(struct SerializerBuffer& buffer, struct %s& data); \n", linking, name.full, name.type);
        fprintf(f, "void write_%s_to_buffer(SerializerBuffer& buffer, %s& data) {\n", name.full, name.type);
        
        if (trivial) {
            fprintf(f, "    write_n_to_buffer(buffer, &data, sizeof(%s));\n", name.type);
        }
        else {
            for (Field& field : type->fields) {
                char write_to[100];
                snprintf(write_to, 100, "data.%s", field.name);

                if (field.type->type == Type::Union && field.name == "") {
                    serialize_tagged_union(f, name.type, (UnionType*)field.type, tag_type, write_to, serialize_type);
                }
                else {
                    serialize_type(f, field.type, write_to);
                }
            }
        }
        fprintf(f, "}\n\n");

        fprintf(h, "%s void read_%s_from_buffer(struct DeserializerBuffer& buffer, struct %s& data); \n", linking, name.full, name.type);
        fprintf(f, "void read_%s_from_buffer(DeserializerBuffer& buffer, %s& data) {\n", name.full, name.type);

        if (trivial) {
            read_trivial_from_buffer(f, name.type, "data");
        }
        else {
            for (Field& field : type->fields) {
                char write_to[100];
                snprintf(write_to, 100, "data.%s", field.name);

                if (field.type->type == Type::Union && field.name == "") {
                    serialize_tagged_union(f, name.type, (UnionType*)field.type, tag_type, write_to, deserialize_type);
                }
                else if (is_tagged_union && strcmp(field.name, "type") == 0) {
                    fprintf(f, "    ");
                    dump_type(f, tag_type);
                    fprintf(f, " type;\n");
                    deserialize_type(f, field.type, "type");
                    fprintf(f, "    new (&data) %s(type);\n", name.type);

                }
                else {
                    deserialize_type(f, field.type, write_to);
                }
            }
        }
        fprintf(f, "}\n\n");

        fprintf(h, "%s refl::Struct* get_%s_type();\n", linking, name.full);

        fprintf(f, "refl::Struct* get_%s_type() {\n", name.full);
        fprintf(f, "	static refl::Struct type = init_%s();\n", name.full);
        fprintf(f, "	return &type;\n");
        fprintf(f, "}\n\n");

        
    }

    void dump_alias_reflector(FILE* f, FILE* h, AliasType& alias, const char* linking) {
        Name& name = alias.name;
        fprintf(h, "%s refl::Alias* get_%s_type();\n", linking, name.full);
        fprintf(f, "refl::Alias* get_%s_type() {\n", name.full);
        fprintf(f, "    static refl::Alias type(\"%s\", ", name.full);
        dump_type_ref(f, alias.aliasing);
        fprintf(f, ");\n");
        fprintf(f, "    return &type;\n");
        fprintf(f, "}\n\n");


        fprintf(h, "%s void write_%s_to_buffer(SerializerBuffer& buffer, ", linking, name.full);
        dump_type(h, alias.aliasing);
        fprintf(h, "& data);\n");

        fprintf(f, "void write_%s_to_buffer(SerializerBuffer& buffer, %s& data) {\n", name.full, name.type);
        serialize_type(f, alias.aliasing, "data");
        fprintf(f, "}\n");

        fprintf(h, "%s void read_%s_from_buffer(DeserializerBuffer& buffer, ", linking, name.full);
        dump_type(h, alias.aliasing);
        fprintf(h, "& data);\n");

        fprintf(f, "void read_%s_from_buffer(DeserializerBuffer& buffer, %s& data) {\n", name.full, name.type);
        deserialize_type(f, alias.aliasing, "data");
        fprintf(f, "}\n");
    }

    void dump_reflector(FILE* f, FILE* h, Reflector& reflector, Namespace* space, string prefix, int indent = 0) {
        const char* linking = reflector.linking;
        
        if (space->structs.length == 0 && space->namespaces.length == 0 && space->enums.length == 0) return;
        
        char buffer[100];
        to_cstr(prefix, buffer, 100);
        to_cstr(space->name, buffer + prefix.length, 100);
        
        bool is_root = space->name.length == 0;
        if (!is_root) {
            to_cstr("::", buffer + prefix.length + space->name.length, 100);
        }
        
        for (int i = 0; i < space->namespaces.length; i++) {
            dump_reflector(f, h, reflector, space->namespaces[i], buffer, indent);
        }

        for (AliasType* type : space->alias) dump_alias_reflector(f, h, *type, linking);
        for (EnumType* type : space->enums) dump_enum_reflector(f, h, type, linking);
        for (StructType* type : space->structs) dump_struct_reflector(f, h, type, buffer, reflector, indent);
    }

    void assign_components_ids(Reflector& ref) {
        Namespace* space = ref.namespaces.last();

        uint id_counter = string(ref.linking) == "ENGINE_API" ? 0 : 30; //todo get real number of components

        //TODO save in file
        ref.name_to_id["Entity"] = 0;
        ref.name_to_id["Materials"] = 20;
        ref.name_to_id["ModelRenderer"] = 21;
        ref.name_to_id["STATIC"] = 22;
        ref.name_to_id["EDITOR_ONLY"] = 23;

        bool used_ids[MAX_COMPONENTS] = {};
        for (auto [name, id] : ref.name_to_id) {
            used_ids[id] = true;
        }

        for (StructType* type : space->structs) {
            if (!(type->flags & COMPONENT_TAG || type->flags & ENTITY_FLAG_TAG)) continue;

            uint* has_id = ref.name_to_id.get(type->name.iden);
            if (!has_id) {
                while (used_ids[id_counter++]) {}
                ref.name_to_id.set(type->name.iden, id_counter - 1);
            }
        }
    }

    void dump_register_components(Reflector& ref, FILE* f) {
        Namespace* space = ref.namespaces.last();

        char header_path[MAX_FILEPATH];
        snprintf(header_path, MAX_FILEPATH, "%s/%s", ref.include, ref.component);

        FILE* h = fopen(header_path, "w");
        if (!h) {
            fprintf(stderr, "Could not open %s file for writing!", header_path);
            exit(1);
            return;
        }

        string_view linking = ref.linking;

        fprintf(h, "#pragma once\n");
        fprintf(h, "#include \"ecs/system.h\"\n\n");

        for (StructType* type : space->structs) {
            if (type->flags & COMPONENT_TAG) {
                uint id = ref.name_to_id[type->name.iden];
                fprintf(h, "DEFINE_COMPONENT_ID(%s, %i)\n", type->name.type, id);
            }
            else if (type->flags & ENTITY_FLAG_TAG) {
                uint id = ref.name_to_id[type->name.iden];
                fprintf(h, "constexpr EntityFlags %s = 1ull << %i;\n", type->name.type, id);
            }
        }

        fprintf(f, "#include \"%s\"\n", header_path);
        fprintf(f, "#include \"ecs/ecs.h\"\n");
        fprintf(f, "#include \"engine/application.h\"\n\n");

        if (linking == "ENGINE_API") fprintf(f, "\nvoid register_default_components(World& world) {\n");
        else fprintf(f, "\nAPPLICATION_API void register_components(World& world) {\n");
        
        uint count = 0;
        for (StructType* type : space->structs) {
            if (type->flags & COMPONENT_TAG) count++;
        }
        
        fprintf(f, "    RegisterComponent components[%i] = {};\n", count);

        count = 0;
        for (StructType* type : space->structs) {
            if (!(type->flags & COMPONENT_TAG)) continue;

            uint id = ref.name_to_id[type->name.iden];

            Name& name = type->name;

            char component[100];
            snprintf(component, 100, "    components[%i]", count);

            fprintf(f, "%s.component_id = %i;\n", component, id);
            fprintf(f, "%s.type = get_%s_type();\n", component, name.full);
            if ((type->flags & ENTITY_FLAG_TAG) == ENTITY_FLAG_TAG) {
                fprintf(f, "%s.kind = FLAG_COMPONENT;\n", component);
                continue;
            }
            else if ((type->flags & SYSTEM_COMPONENT_TAG) == SYSTEM_COMPONENT_TAG) {
                fprintf(f, "%s.kind = SYSTEM_COMPONENT;\n", component);
            }
            
            fprintf(f, "%s.funcs.constructor = [](void* data, uint count) { for (uint i=0; i<count; i++) new ((%s*)data + i) %s(); };\n", component, name.type, name.type);

            if (!is_trivially_copyable(ref, type)) {
                fprintf(f, "%s.funcs.copy = [](void* dst, void* src, uint count) { for (uint i=0; i<count; i++) new ((%s*)dst + i) %s(((%s*)src)[i]); };\n", component, name.type, name.type, name.type);
                fprintf(f, "%s.funcs.destructor = [](void* ptr, uint count) { for (uint i=0; i<count; i++) ((%s*)ptr)[i].~%s(); };\n", component, name.type, name.iden);
                fprintf(f, "%s.funcs.serialize = [](SerializerBuffer& buffer, void* data, uint count) {\n", component);
                fprintf(f, "        for (uint i = 0; i < count; i++) write_%s_to_buffer(buffer, ((%s*)data)[i]);\n", name.full, name.type);
                fprintf(f, "    };\n");
                fprintf(f, "%s.funcs.deserialize = [](DeserializerBuffer& buffer, void* data, uint count) {\n", component);
                fprintf(f, "        for (uint i = 0; i < count; i++) read_%s_from_buffer(buffer, ((%s*)data)[i]);\n", name.full, name.type);
                fprintf(f, "    };\n");
                fprintf(f, "\n\n");
            }
            
            count++;
            
            //fprintf(f, "     world.")
        }
        
        fprintf(f, "    world.register_components({components, %i});\n", count);

        fprintf(f, "\n};");
    }

    //DESERIALIZE FROM BUFFER
    const char* read_cstring_from_buffer(DeserializerBuffer& buffer) {
        uint length = 0;
        read_uint_from_buffer(buffer, length);
        
        char* str = TEMPORARY_ARRAY(char, length + 1);
        read_n_from_buffer(buffer, str, length);
        str[length] = '\0';

        return str;
    }

    void read_name_from_buffer(DeserializerBuffer& buffer, Name& name) {
        name.full = read_cstring_from_buffer(buffer);
        name.iden = read_cstring_from_buffer(buffer);
        name.type = read_cstring_from_buffer(buffer);
    }

    Type* read_type_from_buffer(DeserializerBuffer& buffer) {
        uint kind;
        read_uint_from_buffer(buffer, kind);

        switch (kind) {
        case Type::Char: return &char_type;
        case Type::Bool: return &bool_type;
        case Type::Uint: return &uint_type;
        case Type::Int: return &int_type;
        case Type::Float: return &float_type;
        case Type::Double: return &double_type;
        case Type::I64: return &i64_type;
        case Type::U64: return &u64_type;
        case Type::SString: return &sstring_type;
        case Type::StringBuffer: return &string_buffer_type;
        case Type::StringView: return &string_view_type;
        case Type::Array: {
            Array* type = TEMPORARY_ALLOC(Array);
            type->type = Type::Array;
            uint kind;
            read_uint_from_buffer(buffer, kind);

            type->arr_type = (Array::ArrayType)kind;
            read_uint_from_buffer(buffer, type->num);
            type->element = read_type_from_buffer(buffer);
            return type;
        }
        case Type::Ptr: {
            Ptr* ptr = TEMPORARY_ALLOC(Ptr);
            ptr->type = Type::Ptr;
            ptr->element = read_type_from_buffer(buffer);
            return ptr;
        }

        case Type::Union:
        case Type::Alias:
        case Type::Struct:
        case Type::Enum:
        case Type::StructRef: {
            StructRef* type = TEMPORARY_ALLOC(StructRef);
            type->type = Type::StructRef;
            read_name_from_buffer(buffer, type->name);
            return type;
        }
        }
    }

    void read_fields_from_buffer(DeserializerBuffer& buffer, tvector<Field>& fields) {
        read_uint_from_buffer(buffer, fields.length);
        fields.reserve(fields.length);

        for (Field& type : fields) {
            type.name = read_cstring_from_buffer(buffer);
            read_uint_from_buffer(buffer, type.flags);
            type.type = read_type_from_buffer(buffer);
        }
    }

    void read_constant_from_buffer(DeserializerBuffer& buffer, Constant& constant) {
        constant.name = read_cstring_from_buffer(buffer);
        read_int_from_buffer(buffer, constant.value);
    }

    void read_enum_from_buffer(DeserializerBuffer& buffer, EnumType* type) {
        read_name_from_buffer(buffer, type->name);
        read_uint_from_buffer(buffer, type->flags);
        read_n_from_buffer(buffer, &type->is_class, 1);

        read_uint_from_buffer(buffer, type->values.length);
        type->values.reserve(type->values.length);

        for (Constant& constant : type->values) {
            read_constant_from_buffer(buffer, constant);
        }
    }

    void read_union_from_buffer(DeserializerBuffer& buffer, UnionType* type) {
        read_name_from_buffer(buffer, type->name);
        read_uint_from_buffer(buffer, type->flags);

        read_fields_from_buffer(buffer, type->fields);
    }

    void read_alias_from_buffer(DeserializerBuffer& buffer, AliasType* type) {
        read_name_from_buffer(buffer, type->name);
        read_uint_from_buffer(buffer, type->flags);
        type->aliasing = read_type_from_buffer(buffer);
    }

    void read_struct_from_buffer(DeserializerBuffer& buffer, StructType* type) {
        read_name_from_buffer(buffer, type->name);
        read_uint_from_buffer(buffer, type->flags);
        read_fields_from_buffer(buffer, type->fields);
    }

    void read_type_info_from_buffer(DeserializerBuffer& buffer, Namespace* space) {
        space->name = read_cstring_from_buffer(buffer);
        read_uint_from_buffer(buffer, space->namespaces.length);
        space->namespaces.reserve(space->namespaces.length);
        for (uint i = 0; i < space->namespaces.length; i++) {
            Namespace* sub = TEMPORARY_ALLOC(Namespace);
            read_type_info_from_buffer(buffer, sub);

            space->namespaces[i] = sub;
        }

        read_uint_from_buffer(buffer, space->structs.length);
        space->structs.reserve(space->structs.length);
        
        for (uint i = 0; i < space->structs.length; i++) {
            StructType* type = TEMPORARY_ALLOC(StructType);
            type->type = Type::Struct;
            read_struct_from_buffer(buffer, type);

            space->structs[i] = type;
        }

        read_uint_from_buffer(buffer, space->enums.length);
        space->enums.reserve(space->enums.length);
        for (uint i = 0; i < space->enums.length; i++) {
            EnumType* type = TEMPORARY_ALLOC(EnumType);
            type->type = Type::Enum;
            read_enum_from_buffer(buffer, type);

            space->enums[i] = type;
        }

        read_uint_from_buffer(buffer, space->alias.length);
        space->alias.reserve(space->alias.length);

        for (uint i = 0; i < space->alias.length; i++) {
            AliasType* type = TEMPORARY_ALLOC(AliasType);
            type->type = Type::Alias;
            read_alias_from_buffer(buffer, type);

            space->alias[i] = type;
        }

        read_uint_from_buffer(buffer, space->unions.length);
        space->unions.reserve(space->unions.length);
        
        for (uint i = 0; i < space->unions.length; i++) {
            UnionType* type = TEMPORARY_ALLOC(UnionType);
            type->type = Type::Union;
            read_union_from_buffer(buffer, type);

            space->unions[i] = type;
        }
    }

    //SERIALIZE TO BUFFER

    void write_cstring_to_buffer(SerializerBuffer& buffer, const char* str) {
        uint length = strlen(str);
        write_uint_to_buffer(buffer, length);
        write_n_to_buffer(buffer, (void*)str, length);
    }

    void write_name_to_buffer(SerializerBuffer& buffer, Name& name) {
        write_cstring_to_buffer(buffer, name.full);
        write_cstring_to_buffer(buffer, name.iden);
        write_cstring_to_buffer(buffer, name.type);
    }

    void write_type_to_buffer(SerializerBuffer& buffer, Type* type) {
        write_uint_to_buffer(buffer, type->type);
        
        switch (type->type) {
        case Type::Array: {
            Array* array = (Array*)type;

            write_uint_to_buffer(buffer, array->arr_type);
            write_uint_to_buffer(buffer, array->num);
            write_type_to_buffer(buffer, array->element);
            
            break;
        }
        case Type::Union:
        case Type::Alias:
        case Type::Struct:
        case Type::Enum:
        case Type::StructRef:
            write_name_to_buffer(buffer, ((StructRef*)type)->name);
        }
    }

    void write_fields_to_buffer(SerializerBuffer& buffer, tvector<Field>& fields) {
        write_uint_to_buffer(buffer, fields.length);
        
        for (Field& type : fields) {
            write_cstring_to_buffer(buffer, type.name);
            write_uint_to_buffer(buffer, type.flags);
            write_type_to_buffer(buffer, type.type);
        }
    }

    void write_constant_to_buffer(SerializerBuffer& buffer, Constant& constant) {
        write_cstring_to_buffer(buffer, constant.name);
        write_int_to_buffer(buffer, constant.value);
    }

    void write_enum_to_buffer(SerializerBuffer& buffer, EnumType* type) {
        write_name_to_buffer(buffer, type->name);
        write_uint_to_buffer(buffer, type->flags);
        write_char_to_buffer(buffer, type->is_class);
        
        write_uint_to_buffer(buffer, type->values.length);
        for (Constant& constant : type->values) {
            write_constant_to_buffer(buffer, constant);
        }
    }

    void write_union_to_buffer(SerializerBuffer& buffer, UnionType* type) {
        write_name_to_buffer(buffer, type->name);
        write_uint_to_buffer(buffer, type->flags);
        write_fields_to_buffer(buffer, type->fields);
    }

    void write_alias_to_buffer(SerializerBuffer& buffer, AliasType* type) {
        write_name_to_buffer(buffer, type->name);
        write_uint_to_buffer(buffer, type->flags);
        write_type_to_buffer(buffer, type->aliasing);
    }

    void write_struct_to_buffer(SerializerBuffer& buffer, StructType* type) {
        write_name_to_buffer(buffer, type->name);
        write_uint_to_buffer(buffer, type->flags);
        write_fields_to_buffer(buffer, type->fields);
    }

    void write_type_info_to_buffer(SerializerBuffer& buffer, Namespace* space) {
        write_uint_to_buffer(buffer, space->name.length);
        write_n_to_buffer(buffer, (void*)space->name.data, space->name.length);

        write_uint_to_buffer(buffer, space->namespaces.length);
        for (Namespace* sub : space->namespaces) {
            write_type_info_to_buffer(buffer, sub);
        }

        write_uint_to_buffer(buffer, space->structs.length);
        for (StructType* type : space->structs) {
            write_struct_to_buffer(buffer, type);
        }

        write_uint_to_buffer(buffer, space->enums.length);
        for (EnumType* type : space->enums) {
            write_enum_to_buffer(buffer, type);
        }

        write_uint_to_buffer(buffer, space->alias.length);
        for (AliasType* type : space->alias) {
            write_alias_to_buffer(buffer, type);
        }

        write_uint_to_buffer(buffer, space->unions.length);
        for (UnionType* type : space->unions) {
            write_union_to_buffer(buffer, type);
        }
    }

    struct FieldDiff {
        enum Mode { None };
    };

    void diff_fields(tvector<Field>& past, tvector<Field>& current) {
        for (uint i = 0; i < current.length; i++) {
            Field& field = current[i];
            Field* other = nullptr;

            if (i < past.length && !strcmp(past[i].name, field.name)) other = &past[i];
            else {
                for (Field& c : past) {
                    if (!strcmp(c.name, field.name)) {
                        other = &past[i];
                        break;
                    }
                }
            }

            if (other == nullptr) {

            }
        }
    }

    void diff_namespace(Namespace* past_space, Namespace* current) {
        for (Namespace* inner : current->namespaces) {

        }
        
        for (StructType* type : current->structs) {
            Type* past = find_type(*past_space, type->name.full);
            if (past == NULL) continue;
            if (past->type != Type::Struct) {
                fprintf(stderr, "Changing the underlying type of %s ", past, " will break all save files");
            }


        }

        for (EnumType* type : current->enums) {

        }

        for (UnionType* type : current->unions) {

        }

        for (AliasType* type : current->alias) {

        }
    }

    void dump_reflector(Reflector& ref) {
        Namespace* space = ref.namespaces.last();


        char cpp_output_file[MAX_FILEPATH];
        char h_output_file[MAX_FILEPATH];
        char type_output_path[MAX_FILEPATH];

        snprintf(h_output_file, MAX_FILEPATH, "%s/%s.h", ref.include, ref.h_output);
        snprintf(cpp_output_file, MAX_FILEPATH, "%s/%s.cpp", ref.base, ref.output);
        snprintf(type_output_path, MAX_FILEPATH, "%s/%s", ref.base, "type_info.refl");

        FILE* file = fopen(cpp_output_file, "w");

        FILE* header_file = fopen(h_output_file, "w");

        FILE* type_info_file = fopen(type_output_path, "r");

        if (!file) {
            fprintf(stderr, "Could not open cpp output file \"%s\"\n", cpp_output_file);
            fclose(file);
            return;
        }

        if (!header_file) {
            fprintf(stderr, "Could not open header output file \"%s\"\n", h_output_file);
            fclose(file);
            return;
        }

        if (!type_info_file) type_info_file = nullptr;

        fprintf(header_file, "#pragma once\n#include \"core/core.h\"\n\n");

        fprintf(file, "#include \"%s.h\"\n", ref.h_output);
        if (strcmp(ref.linking, "ENGINE_API") != 0) { //todo allow adding includes via command line
            fprintf(file, "#include \"engine/types.h\"\n");
        }
        else {
            fprintf(header_file, "#include \"engine/core.h\"\n");
        }

        fprintf(header_file, "#include \"core/memory/linear_allocator.h\"\n");
        fprintf(header_file, "#include \"core/serializer.h\"\n");
        fprintf(header_file, "#include \"core/reflection.h\"\n");
        fprintf(header_file, "#include \"core/container/array.h\"\n");

        for (int i = 0; i < ref.header_files.length; i++) {
            fprintf(file, "#include \"%s/%s\"\n", ref.base, ref.header_files[i]);
        }

        fprintf(file, "\n");
        fprintf(header_file, "\n");

        //fprintf(file, "LinearAllocator reflection_allocator(kb(512));\n\n");
        
        //todo turn into function
        //todo free allocated buffer memory, requires two pools
        if (type_info_file){
            struct _stat info;
            _stat(type_output_path, &info);
        
            uint capacity = info.st_size;

            DeserializerBuffer buffer = {};
            buffer.data = TEMPORARY_ARRAY(char, capacity);
            buffer.length = fread(buffer.data, sizeof(char), capacity, type_info_file);
            
            Namespace past_space;
            read_type_info_from_buffer(buffer, &past_space);

            diff_namespace(&past_space, space);

            fclose(type_info_file);
        }

        assign_components_ids(ref);

        dump_reflector(file, header_file, ref, space, "", 0);

        dump_register_components(ref, file);
        
        //fprintf(file, "\t}\n}\n");
        //fprintf(header_file, "\t}\n}\n");
        fclose(file);
        fclose(header_file);
        

        //todo move into a function
        {
            SerializerBuffer buffer = {};
            buffer.capacity = mb(5);
            buffer.data = TEMPORARY_ARRAY(char, buffer.capacity);

            write_type_info_to_buffer(buffer, space);

            FILE* type_info_file = fopen(type_output_path, "w");
            if (!type_info_file) {
                printf("Could not open type_info file");
                return;
            }

            fwrite(buffer.data, sizeof(char), buffer.index, type_info_file);
            fclose(type_info_file);
        }

    }
};
    


using namespace pixc;
using namespace pixc::reflection::compiler;

#ifdef NE_PLATFORM_WINDOWS
#define WIN32_MEAN_AND_LEAN
#include <windows.h>

void listdir(const char* base, const char *path, int indent, tvector<const char*>& header_paths) {
	bool is_root = strlen(path) == 0;
	const char* indent_str = "                           ";

	char search[100];
	sprintf_s(search, "%s/%s/*", base, path);

	_WIN32_FIND_DATAA find_file_data;
	HANDLE hFind = FindFirstFileA(search, &find_file_data);
	if (hFind == INVALID_HANDLE_VALUE) {
		printf("%.*sempty\n", indent, indent_str);
		return;
	}

	do {
		string_view file_name = find_file_data.cFileName;
		bool is_directory = find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

		char buffer[100];

		if (is_directory && !file_name.starts_with(".")) {
			char search[500];
			sprintf_s(search, "%s/%s", path, file_name.data);

			printf("%.*s%s/\n", indent, indent_str, file_name.data);
			listdir(base, search, indent + 4, header_paths);
		}
		else if (file_name.ends_with(".h")) {
			char* buffer = TEMPORARY_ARRAY(char, 100);
			
			if (is_root) sprintf_s(buffer, 100, "%s", file_name.data);
			else sprintf_s(buffer, 100, "%s/%s", path, file_name.data);
			
			header_paths.append(buffer);

			printf("%.*s%s\n", indent, indent_str, file_name.data);
		}
	} while (FindNextFileA(hFind, &find_file_data) != 0);

	FindClose(hFind);
}
#endif

#ifdef NE_PLATFORM_MACOSX
#include <dirent.h>

void listdir(const char* base, const char *path, int indent, tvector<const char*>& header_paths) {
    bool is_root = strlen(path) == 0;
    const char* indent_str = "                           ";

    char search[100];
    snprintf(search, 100, "%s/%s", base, path);

    struct dirent * entry;
    DIR* dp = opendir(search);
    
    //printf("Searching through %s\n", search);
    
    if (!dp) {
        fprintf(stderr, "%.*sCould not open directory %s!\n", indent, indent_str, search);
        exit(1);
        return;
    }

    while (entry = readdir(dp)) {
        string_view file_name = entry->d_name;
        bool is_directory = entry->d_type == DT_DIR;

        char buffer[100];

        if (file_name.ends_with(".xcodeproj")
         || file_name == "targetver.h"
         || file_name == "stdafx.h") {
            continue;
        }
        else if (is_directory && !file_name.starts_with(".")) {
            char search[500];
            snprintf(search, 500, "%s/%s", path, file_name.data);

            printf("%.*s%s/\n", indent, indent_str, file_name.data);
            listdir(base, search, indent + 4, header_paths);
        }
        else if (file_name.ends_with(".h")) {
            char* buffer = TEMPORARY_ARRAY(char, 100);
            
            if (is_root) snprintf(buffer, 100, "%s", file_name.data);
            else snprintf(buffer, 100, "%s/%s", path, file_name.data);
            
            header_paths.append(buffer);

            printf("%.*s%s\n", indent, indent_str, file_name.data);
        }
    }

    closedir(dp);
}
#endif

//#include "core/profiler.h"

//todo move functions to read files into core, asap!
FILE* open(string_view full_filepath, const char* mode) {
	return fopen(full_filepath.c_str(), mode);
}

bool io_readf(string_view full_filepath, string_buffer* buffer, int null_terminated = true) {
	FILE* f = open(full_filepath, "rb");
	if (!f) return false;

	struct _stat info[1];

	_stat(full_filepath.c_str(), info);
	size_t length = info->st_size;

	buffer->reserve(length + null_terminated);
	length = fread(buffer->data, sizeof(char), length, f);
	if (null_terminated) buffer->data[length] = '\0';
	buffer->length = length;

	fclose(f);

	return true;
}

#include "core/context.h"

int main(int argc, const char** c_args) {
	LinearAllocator& permanent_allocator = get_thread_local_permanent_allocator(); 
	LinearAllocator& temporary_allocator = get_thread_local_temporary_allocator();
	
	permanent_allocator = LinearAllocator(mb(10));
	temporary_allocator = LinearAllocator(mb(10));

	Context context;
	context.temporary_allocator = &get_thread_local_temporary_allocator();
	context.allocator = &default_allocator;

	ScopedContext scoped_context(context);    

    error::Error err = {};
    
    Reflector reflector = {};
    reflector.err = &err;
	reflector.allocator = &permanent_allocator;
	reflector.linking = "";
	reflector.base = ".";
    reflector.output = "generated";
	reflector.include = "\include";
	reflector.h_output = "generated";
	reflector.component = "component_ids.h";
    
    Namespace* root = make_Namespace(reflector, "");
	reflector.namespaces.append(root);
    
    lexer::init();

	printf("==== INPUT FILES ====\n");

	bool modified = false;

	//Profile input_profile("INPUT PROFILE");

	tvector<const char*> dirs;

    for (int i = 1; i < argc; i++) {
        string arg = c_args[i];
        
		//todo handle bad input!
        if (arg == "-o") {
            reflector.output = c_args[++i];
        }

		else if (arg == "-r") {
			modified = true;
		}

		else if (arg == "-h") {
			const char* filename = c_args[++i];
			reflector.h_output = filename;
		}
        
        else if (arg == "-c") {
            const char* filename = c_args[++i];
            reflector.component = filename;
        }

		else if (arg == "-d") {
			const char* filename = c_args[++i];
			printf("%s/\n", filename);
			dirs.append(filename);
		}

		else if (arg == "-b") {
			reflector.base = c_args[++i];
		}

		else if (arg == "-i") {
			reflector.include = c_args[++i];
		}

		else if (arg == "-l") {
			reflector.linking = c_args[++i];
			printf("%s\n", reflector.linking);
		}

		else if (arg == "-c") {
			reflector.component = c_args[++i];
		}
        
		else {
			printf("%s\n", arg.c_str());
			reflector.header_files.append(arg.c_str());
		}
    }

	//todo check for buffer overruns
	char* full_include = TEMPORARY_ARRAY(char, MAX_FILEPATH);
	snprintf(full_include, MAX_FILEPATH, "%s/%s", reflector.base, reflector.include);
	reflector.include = full_include;

	for (const char* dir : dirs) {
		listdir(reflector.include, dir, 0, reflector.header_files);
	}

	if (!modified) {
		char generated_cpp_filename[200];
		snprintf(generated_cpp_filename, 200, "%s.cpp", reflector.output);

        struct _stat output_info;
        if (_stat(generated_cpp_filename, &output_info) == -1) output_info = {};

		//todo replace with macro generated by the build system
		const char* OUTPUT_FILE = "C:\\Users\\User\\source\\repos\\NextEngine\\x64\\Release\\ReflectionTool.exe";

		struct _stat exe_info;
        if (_stat(OUTPUT_FILE, &exe_info) == -1) exe_info = {};

		if (exe_info.st_mtime > output_info.st_mtime) modified = true;

		for (string_view filename : reflector.header_files) {
			char full_path[200];
			snprintf(full_path, 200, "%s/%s", reflector.include, filename.c_str());
			
			struct _stat info;
			_stat(full_path, &info);

			if (info.st_mtime > output_info.st_mtime) {
				modified = true;
				break;
			}
		}

		if (!modified) {
			printf("\n=== NO FILE MODIFIED ===\n");
			return 0;
		}
	}

	//printf("=== SEARCHING DIRECTORIES TOOK %f", input_profile.end());

	printf("\n=== PARSING ===\n");

	//Profile parsing_profile("PARSING");

	for (string_view filename : reflector.header_files) {
		char full_path[MAX_FILEPATH];
		snprintf(full_path, MAX_FILEPATH, "%s/%s", reflector.include, filename.c_str());
		
		string_buffer contents;
		contents.allocator = &get_temporary_allocator();
		if (!io_readf(full_path, &contents)) {
			fprintf(stderr, "Could not open file! %s", full_path);
			exit(1);
		}


		err.filename = filename;
		err.src = contents;

		lexer::Lexer lexer = {};
		slice<lexer::Token> tokens = lexer::lex(lexer, contents, &err);

		//lexer::dump_tokens(lexer);

		reflector.i = 0;
		reflector.tokens = tokens;
		parse(reflector);
	}

	//printf("=== PARSING TOOK %f", parsing_profile.end());

    
	if (error::is_error(&err)) {
		error::log_error(&err);
		exit(1);
	}
    
    printf("\n==== GENERATING REFLECTION FILE ====\n");

	//Profile dump_profiler("DUMP");
    dump_reflector(reflector);

	//printf("=== DUMP TOOK %f", dump_profiler.end());
    //dump_reflector_header(reflector);
}

