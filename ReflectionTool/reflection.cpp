//
//  reflection.cpp
//  PixC
//
//  Created by Lucas Goetz on 29/10/2019.
//  Copyright © 2019 Lucas Goetz. All rights reserved.
//

#include "core/container/tvector.h"
#include "core/container/string_view.h"
#include "core/memory/allocator.h"
#include "core/memory/linear_allocator.h"
#include "core/io/vfs.h"
#include "lexer.h"
#include "core/io/vfs.h"
#include "error.h"
#include "helper.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

using string = string_view;

//todo eliminate namespaces!
namespace pixc {
    namespace reflection {
        namespace compiler {
            struct Type {
                enum {Int,Uint,I64,U64,Bool,Float,Char,StringView,SString,StringBuffer,Struct,Enum,StructRef,Array,Ptr,IntArg} type;
            };

            struct Field {
                const char* name;
                Type* type;
            };

			struct Name {
				const char* iden;
				const char* type;
				const char* full;
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
                int num;
            };

			const uint REFLECT_TAG = 1 << 0;
			const uint SERIALIZE_TAG = 1 << 1;
			const uint PRINTABLE_TAG = 1 << 2;
			const uint COMPONENT_TAG = (1 << 3) | REFLECT_TAG | SERIALIZE_TAG;
			const uint TAGGED_UNION_TAG = 1 << 4;
            
			struct Constant {
				const char* name;
				int value;
			};

			struct EnumType : Type {
				Name name;
				tvector<Constant> values;
				bool is_class;
			};

            struct StructType : Type {
				Name name;
                tvector<Field> fields;
                tvector<Type*> template_args;
                unsigned int flags;
            };

            struct Namespace {
                string name;
                tvector<Namespace*> namespaces;
                tvector<StructType*> structs;
				tvector<EnumType*> enums;
            };

            struct Reflector {
                error::Error* err;
                slice<lexer::Token> tokens;
                int i = 0;
                tvector<Namespace*> namespaces;
                tvector<const char*> header_files;
                const char* output;
				const char* linking;
                int component_id;
				LinearAllocator* allocator;
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

            Namespace* make_Namespace(Reflector& ref, string name) {
                Namespace* space = alloc<Namespace>(ref.allocator);
                space->name = name;
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

			bool parse_enum(Reflector& ref, uint flags) {
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
						return false;
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
					return true;
				}
				else if (token.type != lexer::SemiColon) {
					unexpected(ref, token);
					return false;
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
					
					if (token.type == lexer::Static || token.value == "REFL_FALSE") {
                        while (ref.tokens[++ref.i].type != lexer::SemiColon) {};
						continue;
                    }
                    else if (token.type == lexer::Struct) {
                        parse_struct(ref, 0); 
						continue;
                    }
					else if (token.type == lexer::Enum) {
						bool is_identifier = parse_enum(ref, IN_STRUCT);
						type = space->enums.last();
						ref.i--;

						if (!is_identifier) continue;
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

						if (token == "REFL") reflect_struct = true;
					}

					if (reflect_struct) {
						if (ref.tokens[ref.i].type == lexer::Struct) {
							parse_struct(ref, flags);
						}
						else if (ref.tokens[ref.i].type == lexer::Enum) {
							parse_enum(ref, flags);
						}
						else {
							unexpected(ref, ref.tokens[ref.i]);
							return;
						}
					}
                }
            }
            
            const char* indent_buffer = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
            
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
                    fprintf(f, "%s", ref->name.type);
                
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

			void serialize_type(FILE* f, Type* type, const char* name) {
				//fprintf(f, "    write_to_buffer(buffer, %s);\n", name);

				switch (type->type) {
				case Type::Enum:
				case Type::Int: 
				case Type::Uint:  
				case Type::Float: 
				case Type::Bool: 
				case Type::StructRef: {
					StructType* struct_type = (StructType*)type;
					fprintf(f, "    write_to_buffer(buffer, %s);\n", name);
					//dump_type(f, type);
					//fprintf(f, ">::serialize(buffer, %s);\n", name);
					break;
				}
				case Type::Array: {
					Array* array_type = (Array*)type;
					char write_to[100];
					char length_str[100];
					sprintf_s(write_to, "%s[i]", name, array_type->num);

					switch (array_type->arr_type) {
					case Array::CArray:
						sprintf_s(length_str, "%i", array_type->num);
						break;
					case Array::Slice:
					case Array::StaticArray:
					case Array::Vector:
						sprintf_s(length_str, "%s.length", name);
					}

					fprintf(f, "	for (uint i = 0; i < %s; i++) {\n     ", length_str);
					serialize_type(f, array_type->element, write_to);
					fprintf(f, "    }\n");
				}
				}
			}

			void dump_enum_reflector(FILE* f, FILE* h, EnumType* type, const char* linking) {
				fprintf(f, "refl::Enum init_%s() {\n", type->name.full);
				fprintf(f, "	refl::Enum type(\"%s\", sizeof(%s));\n", type->name.iden, type->name.type);

				//bool is_tagged_union = type->flags & TAGGED_UNION_TAG;


				for (Constant& constant : type->values) {
					fprintf(f, "	type.values.append({ \"%s\", %s::%s });\n", constant.name, type->name.type, constant.name);
				}

				fprintf(f, "	return type;\n");
				fprintf(f, "}\n\n");

				/*fprintf(h, "void write_to_buffer(struct SerializerBuffer& buffer, struct %s& data); \n", full_type_name_buffer);
				fprintf(f, "void write_to_buffer(SerializerBuffer& buffer, %s& data) {\n", full_type_name_buffer);
				for (Field& field : type->fields) {
					char write_to[100];
					sprintf_s(write_to, "data.%s", field.name.data);

					serialize_type(f, field.type, write_to);
				}
				fprintf(f, "}\n\n");*/

				fprintf(f, "%s refl::Type* refl::TypeResolver<%s>::get() {\n", linking, type->name.type);
				fprintf(f, "	static refl::Enum type = init_%s();\n", type->name.full);
				fprintf(f, "	return &type;\n");
				fprintf(f, "}\n");

				fprintf(h, "template<>\nstruct refl::TypeResolver<%s> {\n", type->name.type);
				fprintf(h, "	%s static refl::Type* get();\n", linking);
				fprintf(h, "};\n\n");
			}

            void dump_struct_reflector(FILE* f, FILE* h, StructType* type, string prefix, const char* linking, int indent = 0) {				
				Name& name = type->name;
				
				fprintf(f, "refl::Struct init_%s() {\n", name.full);
				fprintf(f, "	refl::Struct type(\"%s\", sizeof(%s));\n", name.full, name.type);

				//bool is_tagged_union = type->flags & TAGGED_UNION_TAG;


				for (Field& field : type->fields) {
					fprintf(f, "	type.fields.append({");
					fprintf(f, "\"%s\", offsetof(%s, %s), refl::TypeResolver<", field.name, name.type, field.name);

					dump_type(f, field.type);

					fprintf(f, ">::get() });\n");
				}

				fprintf(f, "	return type;\n");
				fprintf(f, "}\n\n");

				fprintf(h, "void write_to_buffer(struct SerializerBuffer& buffer, struct %s& data); \n", name.type);
				fprintf(f, "void write_to_buffer(SerializerBuffer& buffer, %s& data) {\n", name.type); 
				for (Field& field : type->fields) {
					char write_to[100];
					sprintf_s(write_to, "data.%s", field.name);

					serialize_type(f, field.type, write_to);
				}
				fprintf(f, "}\n\n");

				fprintf(f, "%s refl::Type* refl::TypeResolver<%s>::get() {\n", linking, name.type);
				fprintf(f, "	static refl::Struct type = init_%s();\n", name.full);
				fprintf(f, "	return &type;\n");
				fprintf(f, "}\n");

				fprintf(h, "template<>\nstruct refl::TypeResolver<%s> {\n", name.type);
				fprintf(h, "	%s static refl::Type* get();\n", linking);
				fprintf(h, "};\n\n");
            }

            void dump_reflector(FILE* f, FILE* h, Namespace* space, string prefix, const char* linking, int indent = 0) {
                if (space->structs.length == 0 && space->namespaces.length == 0 && space->enums.length == 0) return;
                
                char buffer[100];
                to_cstr(prefix, buffer, 100);
                to_cstr(space->name, buffer + prefix.length, 100);
                
                bool is_root = space->name.length == 0;
                if (!is_root) {
                    to_cstr("::", buffer + prefix.length + space->name.length, 100);
                }
                
                for (int i = 0; i < space->namespaces.length; i++) {
                    dump_reflector(f, h, space->namespaces[i], buffer, linking,  indent);
                }

				for (EnumType* type : space->enums) {
					dump_enum_reflector(f, h, type, linking);
				}
            
                for (int i = 0; i < space->structs.length; i++) {
                    dump_struct_reflector(f, h, space->structs[i], buffer, linking, indent);
                }
            }

            void dump_reflector(Reflector& ref) {
                Namespace* space = ref.namespaces.last();
                
                char buffer[100];
                to_cstr(ref.output, buffer, 100);
				to_cstr(".cpp", buffer + strlen(ref.output), 100);
                
				FILE* file = nullptr;
				errno_t err_cpp = fopen_s(&file, buffer, "w");
                
                to_cstr(".h", buffer + strlen(ref.output), 100);
                
				FILE* header_file;
				errno_t err_h = fopen_s(&header_file, buffer, "w");
                
                if (err_cpp) {
                    fprintf(stderr, "Could not open cpp output file \"%s\"\n", buffer);
					fclose(file);
					return;
                }
                
                if (err_h) {
                    fprintf(stderr, "Could not open header output file\n");
                    fclose(file);
                    return;
                }
                
				fprintf(header_file, "#pragma once\n#include \"core/core.h\"\n\n");

                fprintf(file, "#include \"%s\"\n", buffer); 
                fprintf(header_file, "#include \"core/memory/linear_allocator.h\"\n");
				fprintf(header_file, "#include \"core/serializer.h\"\n");
                fprintf(header_file, "#include \"core/reflection.h\"\n");
                fprintf(header_file, "#include \"core/container/array.h\"\n");
                
				for (int i = 0; i < ref.header_files.length; i++) {
					fprintf(header_file, "#include \"%s\"\n", ref.header_files[i]);
				}
				
				fprintf(file, "\n");
				fprintf(header_file, "\n");

                //fprintf(file, "LinearAllocator reflection_allocator(kb(512));\n\n");
                
                dump_reflector(file, header_file, space, "", ref.linking, 0);
                
                //fprintf(file, "\t}\n}\n");
                //fprintf(header_file, "\t}\n}\n");
                fclose(file);
                fclose(header_file);
            }
        };
    }
}

using namespace pixc;
using namespace pixc::reflection::compiler;

LinearAllocator temporary_allocator(mb(10));
MallocAllocator default_allocator;

#define WIN32_MEAN_AND_LEAN
#include <windows.h>

void listdir(const char *path, int indent, tvector<const char*>& header_paths) {
	const char* indent_str = "                           ";

	char search[100];
	sprintf_s(search, "%s/*", path);

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
			listdir(search, indent + 4, header_paths);
		}
		else if (file_name.ends_with(".h")) {
			char* buffer = TEMPORARY_ARRAY(char, 100);
			sprintf_s(buffer, 100, "%s/%s", path, file_name.data);
			
			header_paths.append(buffer);

			printf("%.*s%s\n", indent, indent_str, file_name.data);
		}
	} while (FindNextFileA(hFind, &find_file_data) != 0);

	FindClose(hFind);
}

//#include "core/profiler.h"

int main(int argc, const char** c_args) {

	
	LinearAllocator linear_allocator(mb(10));
    
    error::Error err = {};
    
    Reflector reflector = {};
    reflector.err = &err;
    reflector.allocator = &linear_allocator;
	reflector.linking = "";
    
    Namespace* root = make_Namespace(reflector, "");
	reflector.namespaces.append(root);
    
    lexer::init();

	printf("==== INPUT FILES ====\n");

	bool modified = false;

	//Profile input_profile("INPUT PROFILE");

    for (int i = 1; i < argc; i++) {
        string arg = c_args[i];
        
		//todo handle bad input!
        if (arg == "-o") {
            reflector.output = c_args[++i];
        }

		else if (arg == "-r") {
			modified = true;
		}

		else if (arg == "-d") {
			const char* filename = c_args[++i];
			printf("%s/\n", filename);
			listdir(filename, 4, reflector.header_files);
		}

		else if (arg == "-l") {
			reflector.linking = c_args[++i];
			printf("%s\n", reflector.linking);
		}
        
		else {
			printf("%s\n", arg.c_str());
			reflector.header_files.append(arg.c_str());
		}
    }


	if (!modified) {
		char generated_cpp_filename[100];
		sprintf_s(generated_cpp_filename, "%s.cpp", reflector.output);

		struct _stat output_info;
		_stat(generated_cpp_filename, &output_info);

		for (string_view filename : reflector.header_files) {
			struct _stat info;
			_stat(filename.c_str(), &info);

			if (info.st_mtime > output_info.st_mtime) {
				modified = true;
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
		string_buffer contents;
		contents.allocator = &temporary_allocator;
		if (!io_readf(filename, &contents)) {
			fprintf(stderr, "Could not open file! %s", filename.data);
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

