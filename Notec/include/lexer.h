#pragma once

#include "core/core.h"
#include "core/container/string_view.h"
#include "core/container/slice.h"

enum TokenGroup {
    Number,
    String_Literal,
    Keyword,
    Symbol
};

/*using file_handle = uint;

struct Location {
    file_handle file;
    uint line;
    uint column;
};*/

struct Token {
    enum Type {
        Identifier,
        U8, U16, U32, U64, Uint,
        I8, I16, I32, I64, Int,
        F8, F16, F32, F64, Float,
        String,
        Op_Add, Op_Sub, Op_Mul, Op_Div, Op_Mod, Op_Gt, Op_Lt,
        Assign_Add, Assign_Sub, Assign_Mul, Assign_Div, Assign_Mod, Op_Gte, Op_Lte,
        Assign,
        Struct, Class,
        Template, Typename,
        Namespace,
        Using,
        Void,
        BoolType,
        CharType, ShortType, IntType, LongType,
        FloatType, DoubleType,
        Unsigned, Const,
        For, While,
        If, Elif, Else,
        Break, Continue, Operator,
        Return,
        True, False,
        Open_Paren, Close_Paren,
        Open_Bracket, Close_Bracket,
        Open_Square, Close_Square,
        Semicolon, Colon, Comma,
        
        Pre_If, Pre_IfDef, Pre_EndIf, Pre_ElseDef, Pre_Elif, Pre_End, Pre_Include,
        
        End_Of_File,
        Count,
        
        //Ranges
        Op_Begin=Op_Add, Op_End=Op_Mod,
        Assign_Begin=Assign_Add, Assign_End=Assign_Mod,
        Keyword_Begin=Struct, Keyword_End=False,
        Number_Begin = U8, Number_End=Float,
        Preprocessor_Begin = Pre_If, Preprocessor_End = Pre_Include
    } type;
    uint loc;
    
    union {
        string_view value_str = {};
        u64 value_uint;
        i64 value_int;
        double value_float;
    };
};

struct Lexer;

Lexer* make_lexer();
void destroy_lexer(Lexer*);

slice<Token> lex_src(Lexer& lex, string_view src);
void init_lexer_tables();
