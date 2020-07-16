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

const uint MAX_FILEPATH = 200;

//todo eliminate namespaces!
namespace pixc {
    namespace reflection {
        namespace compiler {
            struct Type {
                enum {Int,Uint,I64,U64,Bool,Float,Char,StringView,SString,StringBuffer,Struct,Enum,StructRef,Array,Alias,Ptr,IntArg} type;
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

			struct AliasType : Type {
				Name name;
				Type* aliasing;
				uint flags;
			};

			struct EnumType : Type {
				Name name;
				tvector<Constant> values;
				bool is_class;
				uint flags; //todo make is class a flag
			};

            struct StructType : Type {
				Name name;
                tvector<Field> fields;
                tvector<Type*> template_args;
                uint flags;
            };

            struct Namespace {
                string name;
                tvector<Namespace*> namespaces;
                tvector<StructType*> structs;
				tvector<EnumType*> enums;
				tvector<AliasType*> alias;
			};



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

			void parse_alias(Reflector& ref, uint flags) {
				const char* name = parse_name(ref);
				assert_next(ref, lexer::AssignOp);
				next_token(ref);
				Type* aliasing = parse_type(ref);
				assert_next(ref, lexer::SemiColon);

				make_AliasType(ref, name, aliasing, flags);
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
						lexer::Token token = curr_token(ref);

						switch (token.type) {
						case lexer::Struct: parse_struct(ref, flags); break;
						case lexer::Enum: parse_enum(ref, flags); break;
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
				case Type::Struct: {
					StructType* struct_type = (StructType*)type;

					for (Field& field : struct_type->fields) {
						if (!is_trivially_copyable(reflector, field.type)) return false;
					}

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
					sprintf_s(write_to, "%s[i]", name, array_type->num);

					switch (array_type->arr_type) {
					case Array::CArray:
						sprintf_s(length_str, "%i", array_type->num);
						break;
					case Array::Slice:
					case Array::StaticArray:
					case Array::Vector:
						fprintf(f, "    write_uint_to_buffer(buffer, %s.length);\n", name);
						sprintf_s(length_str, "%s.length", name);
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
					sprintf_s(write_to, "%s[i]", name, array_type->num);

					switch (array_type->arr_type) {
					case Array::CArray:
						sprintf_s(length_str, "%i", array_type->num);
						break;
					case Array::Slice:
					case Array::StaticArray: 
						
					case Array::Vector:
						fprintf(f, "    read_uint_from_buffer(buffer, %s.length);\n", name);
						fprintf(f, "    %s.resize(%s.length);\n", name, name);
						sprintf_s(length_str, "%s.length", name);

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
					case Array::StaticArray: fprintf(f, "make_array_type(%i, ", array->num); break;
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

            void dump_struct_reflector(FILE* f, FILE* h, StructType* type, string prefix, Reflector& reflector, int indent = 0) {				
				Name& name = type->name;
				const char* linking = reflector.linking;
				
				fprintf(f, "refl::Struct init_%s() {\n", name.full);
				fprintf(f, "	refl::Struct type(\"%s\", sizeof(%s));\n", name.full, name.type);

				//bool is_tagged_union = type->flags & TAGGED_UNION_TAG;

				//Support tags
				for (Field& field : type->fields) {
					fprintf(f, "	type.fields.append({");
					fprintf(f, "\"%s\", offsetof(%s, %s), ", field.name, name.type, field.name);
					dump_type_ref(f, field.type);
					
					fprintf(f, "});\n");
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
						sprintf_s(write_to, "data.%s", field.name);

						serialize_type(f, field.type, write_to);
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
						sprintf_s(write_to, "data.%s", field.name);

						deserialize_type(f, field.type, write_to);
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

			void dump_register_components(Reflector& ref, FILE* f) {
				Namespace* space = ref.namespaces.last();

				char header_path[MAX_FILEPATH];
				sprintf_s(header_path, "%s/component_ids.h", ref.include);

				FILE* h = nullptr; 
				errno_t err = fopen_s(&h, header_path, "w");
				if (err || !h) {
					fprintf(stderr, "Could not open %s file for writing!", header_path);
					exit(1);
					return;
				}

				string_view linking = ref.linking;
				uint base_id = linking == "ENGINE_API" ? 1 : 20; //todo get real number of components
				uint id = base_id;

				fprintf(h, "#pragma once\n");
				fprintf(h, "#include \"ecs/system.h\"\n\n");

				for (StructType* type : space->structs) {
					if (!(type->flags & COMPONENT_TAG)) continue;

					if (strcmp(type->name.iden, "Entity") == 0) fprintf(h, "DEFINE_COMPONENT_ID(Entity, 0)\n");
					else fprintf(h, "DEFINE_COMPONENT_ID(%s, %i)\n", type->name.type, id++);
				}

				id = base_id;

				fprintf(f, "#include \"%s\"", header_path);
				fprintf(f, "#include \"ecs/ecs.h\"\n");
				fprintf(f, "#include \"engine/application.h\"\n\n");

				if (linking == "ENGINE_API") fprintf(f, "\nvoid register_default_components(World& world) {\n");
				else fprintf(f, "\nAPPLICATION_API void register_components(World& world) {\n");

				for (StructType* type : space->structs) {
					if (!(type->flags & COMPONENT_TAG)) continue;

					Name& name = type->name;
					bool trivial = is_trivially_copyable(ref, type);

					fprintf(f, "    world.component_type[%i] = get_%s_type();\n", id, name.full);
					fprintf(f, "    world.component_size[%i] = sizeof(%s);\n", id, name.type);
					
					if (!is_trivially_copyable(ref, type)) {
						fprintf(f, "    world.component_lifetime_funcs[%i].copy = [](void* dst, void* src) { new (dst) %s(*(%s*)src); };\n", id, name.type, name.type);
						fprintf(f, "    world.component_lifetime_funcs[%i].destructor = [](void* ptr) { ((%s*)ptr)->~%s(); };\n", id, name.type, name.iden);
						fprintf(f, "    world.component_lifetime_funcs[%i].serialize = [](SerializerBuffer& buffer, void* data, uint count) {\n", id);
						fprintf(f, "        for (uint i = 0; i < count; i++) write_%s_to_buffer(buffer, ((%s*)data)[i]);\n", name.full, name.type);
						fprintf(f, "    };\n");
						fprintf(f, "    world.component_lifetime_funcs[%i].deserialize = [](DeserializerBuffer& buffer, void* data, uint count) {\n", id);
						fprintf(f, "        for (uint i = 0; i < count; i++) read_%s_from_buffer(buffer, ((%s*)data)[i]);\n", name.full, name.type);
						fprintf(f, "    };\n");
						fprintf(f, "\n\n");
					}
					
					//fprintf(f, "     world.")


					id++;
				}

				fprintf(f, "\n};");
			}

            void dump_reflector(Reflector& ref) {
                Namespace* space = ref.namespaces.last();
              

                char cpp_output_file[MAX_FILEPATH];
				char h_output_file[MAX_FILEPATH];

				sprintf_s(h_output_file, "%s/%s.h", ref.include, ref.h_output);
				sprintf_s(cpp_output_file, "%s/%s.cpp", ref.base, ref.output);
                
				FILE* file = nullptr;
				errno_t err_cpp = fopen_s(&file, cpp_output_file, "w");
                                
				FILE* header_file;
				errno_t err_h = fopen_s(&header_file, h_output_file, "w");
                
                if (err_cpp) {
                    fprintf(stderr, "Could not open cpp output file \"%s\"\n", cpp_output_file);
					fclose(file);
					return;
                }
                
                if (err_h) {
                    fprintf(stderr, "Could not open header output file \"%s\"\n", h_output_file);
                    fclose(file);
                    return;
                }
                
				fprintf(header_file, "#pragma once\n#include \"core/core.h\"\n\n");

                fprintf(file, "#include \"%s.h\"\n", ref.h_output); 
				if (strcmp(ref.linking, "ENGINE_API") != 0) { //todo allow adding includes via command line
					fprintf(file, "#include <core/types.h>\n");
				}

                fprintf(header_file, "#include \"core/memory/linear_allocator.h\"\n");
				fprintf(header_file, "#include \"core/serializer.h\"\n");
                fprintf(header_file, "#include \"core/reflection.h\"\n");
                fprintf(header_file, "#include \"core/container/array.h\"\n");
                
				for (int i = 0; i < ref.header_files.length; i++) {
					fprintf(file, "#include \"%s\"\n", ref.header_files[i]);
				}
				
				fprintf(file, "\n");
				fprintf(header_file, "\n");

                //fprintf(file, "LinearAllocator reflection_allocator(kb(512));\n\n");
                
                dump_reflector(file, header_file, ref, space, "", 0);
				dump_register_components(ref, file);
                
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

LinearAllocator temporary_allocator(mb(50));
MallocAllocator default_allocator;

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

//#include "core/profiler.h"


int main(int argc, const char** c_args) {

	
	LinearAllocator linear_allocator(mb(10));
    
    error::Error err = {};
    
    Reflector reflector = {};
    reflector.err = &err;
    reflector.allocator = &linear_allocator;
	reflector.linking = "";
	reflector.base = "";
	reflector.include = "\include";
	reflector.h_output = "generated";
    
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
        
		else {
			printf("%s\n", arg.c_str());
			reflector.header_files.append(arg.c_str());
		}
    }

	//todo check for buffer overruns
	char* full_include = TEMPORARY_ARRAY(char, MAX_FILEPATH);
	sprintf_s(full_include, MAX_FILEPATH, "%s/%s", reflector.base, reflector.include);
	reflector.include = full_include;

	for (const char* dir : dirs) {
		listdir(reflector.include, dir, 0, reflector.header_files);
	}

	if (!modified) {
		char generated_cpp_filename[200];
		sprintf_s(generated_cpp_filename, "%s.cpp", reflector.output);

		struct _stat output_info;
		_stat(generated_cpp_filename, &output_info);

		//todo replace with macro generated by the build system
		const char* OUTPUT_FILE = "C:\\Users\\User\\source\\repos\\NextEngine\\x64\\Release\\ReflectionTool.exe";

		struct _stat exe_info;
		_stat(OUTPUT_FILE, &exe_info);

		if (exe_info.st_mtime > output_info.st_mtime) modified = true;

		for (string_view filename : reflector.header_files) {
			char full_path[200];
			sprintf_s(full_path, "%s/%s", reflector.include, filename.c_str());
			
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
		sprintf_s(full_path, "%s/%s", reflector.include, filename.c_str());
		
		string_buffer contents;
		contents.allocator = &temporary_allocator;
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

