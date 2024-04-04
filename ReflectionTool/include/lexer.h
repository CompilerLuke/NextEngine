//
//  lexer.h
//  TopLangCompiler
//
//  Created by Lucas Goetz on 20/09/2019.
//  Copyright © 2019 Lucas Goetz. All rights reserved.
//

#pragma once

#include "core/container/tvector.h"
#include "core/container/string_view.h"
#include "error.h"

using string = string_view;

namespace pixc {
    namespace lexer {
        enum TokenGroup { Literal, Operator, Symbol, Terminator, Keyword };
        
		//todo enforce naming convention
		enum TokenType {
            MulOp, AssignOp,
            Int, Float, Bool,
            IntType, UintType, I64Type, U64Type, FloatType, F64Type, BoolType, CharType,
			//SStringType, StringViewType, StringBufferType,
            Open_Paren, Close_Paren, DoubleColon, Colon, Open_Bracket, Close_Bracket, Comma, SemiColon, Open_SquareBracket, Close_SquareBracket, GreaterThan, LessThan,
            Identifier,
            Newline, EndOfFile, Open_Indent, Close_Indent,
            Struct, Enum, Union, Class, Using, Namespace, Pragma, Define, Static, Constexpr
        };
        
        struct Token {
            TokenGroup group;
            TokenType type;
            string value;
            unsigned int lbp;
            
            int line;
            int column;
            int length_of_token;
        };
        
        struct Lexer {
            string input;
            
            size_t reserved = 0;
            
            tvector<Token> tokens;
            string tok;

            int i = 0;
            int line = 0;
            int column = 0;
            int indent = 0;
            int indent_diff = 0;
            
            error::Error* err = NULL;
        };
        
        void init();
        void destroy();
        
        string token_type_to_string(TokenType);
        slice<Token> lex(Lexer& lexer, string input, error::Error*);
        void dump_tokens(Lexer& lexer);
        void print_token(const Token& token);
        void destroy(Lexer& lexer);
    }
};
