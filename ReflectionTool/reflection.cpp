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

using string = string_view;

namespace pixc {
    namespace reflection {
        namespace compiler {
            struct Type {
                enum {Int,Bool,Float,Char,Struct,StructRef,Array,Ptr,IntArg} type;
            };

            struct Field {
                string name;
                Type* type;
            };

            struct StructRef : Type {
                string name;
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
                Type* element;
                int num;
            };

			const uint REFLECT_TAG = 1 << 0;
            const uint COMPONENT_TAG = 1 << 1;
            
            struct StructType : Type {
                string name;
                tvector<Field> fields;
                tvector<Type*> template_args;
                unsigned int flags;
            };

            struct Namespace {
                string name;
                tvector<Namespace*> namespaces;
                tvector<StructType*> structs;
            };

            struct Reflector {
                error::Error* err;
                slice<lexer::Token> tokens;
                int i = 0;
                tvector<Namespace*> namespaces;
                tvector<const char*> header_files;
                const char* output;
                int component_id;
				LinearAllocator* allocator;
            };

            void unexpected(Reflector& ref, lexer::Token token) {
                ref.err->column = token.column;
                ref.err->id = error::SyntaxError;
                ref.err->line = token.line;
                ref.err->mesg = "Unexpected";
                ref.err->token_length = token.length_of_token;
                
                error::log_error(ref.err);
                
                exit(1);
            };

            string parse_name(Reflector& ref) {
                lexer::Token tok = ref.tokens[++ref.i];
                if (tok.type != lexer::Identifier) unexpected(ref, tok);
                return tok.value;
            }

            void assert_next(Reflector& ref, lexer::TokenType type) {
                lexer::Token token = ref.tokens[++ref.i];
                
                if (token.type != type) {
                    unexpected(ref, token);
                }
            }

            Type int_type = {Type::Int};
            Type float_type = {Type::Float};
            Type bool_type = {Type::Bool};
            Type char_type = {Type::Char};

            Type* parse_type(Reflector& ref);

            Type* parse_type_inner(Reflector& ref) {
                lexer::Token tok = ref.tokens[ref.i];
                if (tok.type == lexer::IntType) return &int_type;
                if (tok.type == lexer::FloatType) return &float_type;
                if (tok.type == lexer::BoolType) return &bool_type;
                if (tok.type == lexer::CharType) return &char_type;
                
                if (tok.type == lexer::Identifier) {
                    int name_length = 0;
					char* buffer = alloc<char>(ref.allocator, 100);
                    
                    to_cstr(tok.value, buffer, 100);
                    name_length += tok.value.length;
                    
                    while (ref.tokens[ref.i + 1].type == lexer::DoubleColon) {
                        ref.i += 2;
                        tok = ref.tokens[ref.i];
                        
                        string name = tok.value;
                        to_cstr("::", buffer + name_length, 100);
                        name_length += 2;
                        to_cstr(name, buffer + name_length, 100);
                        name_length += name.length;
                    };
                    
                    StructRef* struct_ref = alloc<StructRef>(ref.allocator);
                    struct_ref->type = Type::StructRef;
                    struct_ref->name = buffer;
                    
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
                    }
                    
                
                    return struct_ref;
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

            StructType* make_StructType(Reflector& ref, string name) {
                StructType* struct_type = alloc<StructType>(ref.allocator);
                struct_type->type = Type::Struct;
                struct_type->name = name;
                return struct_type;
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

            void parse_struct(Reflector& ref, unsigned int flags) {
                string name = parse_name(ref);
                if (ref.tokens[ref.i + 1].type == lexer::SemiColon) {
                    ref.i++;
                    return; //forward declaration
                }
                
                assert_next(ref, lexer::Open_Bracket);
                
                StructType* struct_type = make_StructType(ref, name);
                struct_type->flags = flags;
                ref.namespaces.last()->structs.append(struct_type);
                
                Namespace* space = push_Namespace(ref, name);
                
                ref.i++;
                
                for (; ref.tokens[ref.i].type != lexer::Close_Bracket && ref.i < ref.tokens.length; ref.i++) {
                    lexer::Token token = ref.tokens[ref.i];
                    if (token.type == lexer::Static || token.value == "REFLECT_FALSE") {
                        while (ref.tokens[++ref.i].type != lexer::SemiColon) {};
                    }
                    else if (token.type == lexer::Struct) {
                        parse_struct(ref, 0);
                    }
                    else {
                        Type* type = parse_type(ref);
                        string name = parse_name(ref);
                        
                        if (ref.tokens[ref.i + 1].type == lexer::Open_SquareBracket) {
                            ref.i++;
                            Array* arr = alloc<Array>(ref.allocator);
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
                if (type->type == Type::Float) fprintf(f, "float");
                if (type->type == Type::Bool) fprintf(f, "bool");
                if (type->type == Type::StructRef) {
                    StructRef* ref = (StructRef*)type;
                    to_cstr(ref->name, buffer, 100);
                    fprintf(f, "%s", buffer);
                
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
            }
                
            void dump_reflector(FILE* f, StructType* type, string prefix, int indent = 0) {
                char type_name_buffer[100];
                char struct_name_buffer[100];
                char buffer[100];

                to_cstr(type->name, struct_name_buffer, 100);

                fprintf(f, "%.*sStructType init_%s() {\n", indent, indent_buffer, struct_name_buffer);
                
                to_cstr(prefix, type_name_buffer, 100);
                to_cstr(type->name, prefix.length + type_name_buffer, 100);
                
                indent++;
                
                fprintf(f, "%.*sStructType type;\n", indent, indent_buffer);
                
                fprintf(f, "%.*stype.type = Type::Struct;\n", indent, indent_buffer);
                fprintf(f, "%.*stype.name = \"%s\";\n", indent, indent_buffer, struct_name_buffer);
                fprintf(f, "%.*stype.fields.allocator = &reflection_allocator;\n", indent, indent_buffer);
                
                for (Field& field : type->fields) {
                    fprintf(f, "%.*stype.fields.append({", indent, "\t\t\t\t\t\t\t\t\t\t\t");
                    
                    to_cstr(field.name, buffer, 100);

                    fprintf(f, "\"%s\", offsetof(%s, %s), get_type<", buffer, type_name_buffer, buffer);
                    
                    dump_type(f, field.type);
                    
                    fprintf(f, ">() });\n");
                }
                
                fprintf(f, "\n%.*sreturn type;\n", indent, "\t\t\t\t\t\t\t\t\t");
                
                indent--;
                
                fprintf(f, "%.*s}\n\n", indent, "\t\t\t\t\t\t\t\t\t");
                

                fprintf(f, "%.*stemplate<>\nType* get_type<%s>() {\n", indent, indent_buffer, type_name_buffer);
                fprintf(f, "%.*sstatic StructType type = init_%s();\n", indent + 1, indent_buffer, struct_name_buffer);
                fprintf(f, "%.*sreturn &type;\n", indent + 1, indent_buffer);
                fprintf(f, "%.*s};\n\n", indent, indent_buffer);
                
                //fprintf(h, "%.*stemplate<%s> Type* get_type();\n", indent, indent_buffer, type_name_buffer);
            }

            void dump_reflector(FILE* f, FILE* h, Namespace* space, string prefix, int indent = 0) {
                if (space->structs.length == 0 && space->namespaces.length == 0) return;
                
                char buffer[100];
                to_cstr(prefix, buffer, 100);
                to_cstr(space->name, buffer + prefix.length, 100);
                
                bool is_root = space->name.length == 0;
                if (!is_root) {
                    to_cstr("::", buffer + prefix.length + space->name.length, 100);
                }
                
                for (int i = 0; i < space->namespaces.length; i++) {
                    dump_reflector(f, h, space->namespaces[i], buffer, indent);
                }
            
                for (int i = 0; i < space->structs.length; i++) {
                    dump_reflector(f, space->structs[i], buffer, indent);
                }
            }

            void dump_reflector(Reflector& ref) {
                Namespace* space = ref.namespaces.last();
                
                char buffer[100];
                to_cstr(ref.output, buffer, 100);
                to_cstr(".cpp", buffer + strlen(ref.output), 100);
                
				FILE* file;
				errno_t err_cpp = fopen_s(&file, buffer, "w");
                
                //to_cstr(".h", buffer + strlen(ref.output), 100);
                
				//FILE* header_file;
				//errno_t err_h = fopen_s(&header_file, buffer, "w");
                
                if (err_cpp) {
                    fprintf(stderr, "Could not open cpp output file\n");
                    fclose(file);
                    return;
                }
                
                //if (err_h) {
                //   fprintf(stderr, "Could not open header output file\n");
                //    fclose(file);
                //    return;
                //}
                
                //fprintf(file, "#include \"%s\"\n", buffer); #pragma once\n
                fprintf(file, "#include \"core/memory/linear_allocator.h\"\n");
                fprintf(file, "#include \"core/reflection2.h\"\n");
                
				for (int i = 0; i < ref.header_files.length; i++) {
					fprintf(file, "#include \"%s\"\n", ref.header_files[i]);
				}
        

                fprintf(file, "LinearAllocator reflection_allocator(kb(512));\n\n");
                
                dump_reflector(file, nullptr, space, "", 0);
                
                //fprintf(file, "\t}\n}\n");
                //fprintf(header_file, "\t}\n}\n");
                fclose(file);
                //fclose(header_file);
            }
        };
    }
}

using namespace pixc;
using namespace pixc::reflection::compiler;

int main(int argc, const char** c_args) {
    LinearAllocator linear_allocator(mb(10));
    
    error::Error err = {};
    
    Reflector reflector = {};
    reflector.err = &err;
    reflector.allocator = &linear_allocator;
    
    Namespace* root = make_Namespace(reflector, "");
	reflector.namespaces.append(root);
    
    lexer::init();

    for (int i = 1; i < argc; i++) {
        const char* filename = c_args[i];
        
        if (string(filename) == "-o") {
            reflector.output = c_args[++i];
            continue;
        }
        
		string_buffer contents;
		contents.allocator = &temporary_allocator;
		if (!io_readf(filename, &contents)) {
            fprintf(stderr, "Could not open file!");
            exit(1);
        }
                
        reflector.header_files.append(filename);
        
        err.filename = filename;
        err.src = contents;
        
        lexer::Lexer lexer = {};
        slice<lexer::Token> tokens = lexer::lex(lexer, contents, &err);
        
        lexer::dump_tokens(lexer);
        
        reflector.i = 0;
        reflector.tokens = tokens;
        parse(reflector);

        //lexer::destroy(lexer);
    }

	if (error::is_error(&err)) {
		error::log_error(&err);
		exit(1);
	}
    
    printf("==== GENERATING REFLECTION FILE ====\n");
    dump_reflector(reflector);
    //dump_reflector_header(reflector);
}
