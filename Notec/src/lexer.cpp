#include "lexer.h"
#include "stdlib.h"
#include "core/memory/linear_allocator.h"
#include "core/container/string_view.h"
#include "core/container/vector.h"
#include <assert.h>
#include <stdio.h>

struct Lexer {
    string_view src;
    vector<Token> tokens;
    uint i;
    uint line;
    uint column;
};

Lexer* make_lexer() {
    return PERMANENT_ALLOC(Lexer, {});
}

void destroy_lexer(Lexer* lexer) {
    
}

Token make_token(Lexer& lexer, Token::Type type, uint len) {
    Token token = {type};
    token.loc = lexer.i-1;
    //.line = lexer.line;
    //token.loc.column = lexer.column;
    return token;
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_alpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool is_alpha_lower(char c) {
    return c >= 'a' && c <= 'z';
}

bool is_alpha_numeric(char c) {
    return is_digit(c) || is_alpha(c);
}

char last(Lexer& lexer) {
    return lexer.src[lexer.i-1];
}

void adv(Lexer& lexer) {
    if (lexer.i<lexer.src.length) {
        lexer.i++;
        lexer.column++;
    }
}

char next(Lexer& lexer) {
    if (lexer.i < lexer.src.length) {
        lexer.column++;
        return lexer.src[lexer.i++];
    }
    return '\0';
}

char peek(Lexer& lexer) {
    return lexer.i<lexer.src.length ? lexer.src[lexer.i] : '\0';
}

Token lex_number(Lexer& lex) {
    assert(is_digit(last(lex)));
    uint start = lex.i-1;
        
    bool is_real = false;
    int dot_index;
    
    u64 integer_mul = 1;
    
    char c;
    while ((c = peek(lex))) {
        if (c == '.') {
            dot_index = lex.i;
            is_real = true;
        }
        else if (!is_digit(c)) break;
        else if (!is_real) {
            integer_mul *= 10;
        }
        adv(lex);
    }
    uint end = lex.i;
    uint len = end-start;
    
    u64 uint_value = 0;
    
    for (uint i = start; i < (is_real ? dot_index : end); i++) {
        uint_value += integer_mul*(lex.src[i]-'0');
        integer_mul /= 10;
    }
    
    Token token;
    if (is_real) {
        double float_value = uint_value;
        u64 decimal_div = 10;
        
        for (uint i = dot_index+1; i < end; i++) {
            float_value += (double)(lex.src[i]-'0')/decimal_div;
            decimal_div *= 10;
        }
        
        token = make_token(lex, Token::Float, len);
        token.value_float = float_value;
    } else {
        token = make_token(lex, Token::Uint, len);
        token.value_int = uint_value;
    }
    
    return token;
}

const uint MAX_KEYWORDS_PER_LETTER = 10;
const uint LONGEST_KEYWORD = 5;

struct KeywordList {
    uint count;
    string_view keywords[MAX_KEYWORDS_PER_LETTER];
    Token::Type type[MAX_KEYWORDS_PER_LETTER];
};

KeywordList keywords[26];
KeywordList defines[26];

void add_keyword(KeywordList list[26], Token::Type type, const char* str) {
    uint bucket = str[0]-'a';
    uint idx = list[bucket].count++;
    list[bucket].keywords[idx] = str;
    list[bucket].type[idx] = type;
}

bool lookup_match(KeywordList list[26], Lexer& lex, string_view str, Token& token) {
    if (str.length == 0) return false;
    char c = str[0];
    if (is_alpha_lower(c)) {
        KeywordList& bucket = list[c-'a'];
        
        for (uint i = 0; i < bucket.count; i++) {
            if (bucket.keywords[i] == str) {
                token = make_token(lex, bucket.type[i], str.length);
                return true;
            }
        }
    }
    return false;
}

void add_keyword(Token::Type type, const char* str) {
    add_keyword(keywords, type, str);
}

void add_define(Token::Type type, const char* str) {
    assert(str[0] == '#');
    add_keyword(defines, type, str+1);
}

void init_lexer_tables() {
    add_keyword(Token::Struct, "struct");
    add_keyword(Token::Class, "class");
    add_keyword(Token::Template, "template");
    add_keyword(Token::Typename, "typename");
    add_keyword(Token::Using, "using");
    add_keyword(Token::Namespace, "namespace");
    
    add_keyword(Token::Void, "void");
    add_keyword(Token::CharType, "char");
    add_keyword(Token::ShortType, "short");
    add_keyword(Token::IntType, "int");
    add_keyword(Token::LongType, "long");

    add_keyword(Token::Operator, "operator");
    
    add_keyword(Token::FloatType, "float");
    add_keyword(Token::DoubleType, "double");
    
    add_keyword(Token::Unsigned, "unsigned");
    add_keyword(Token::Const, "const");
    
    add_keyword(Token::If, "if");
    add_keyword(Token::Elif, "elif");
    add_keyword(Token::Else, "else");
    add_keyword(Token::While, "while");
    add_keyword(Token::For, "for");
    add_keyword(Token::True, "true");
    add_keyword(Token::False, "false");
    
    add_keyword(Token::Break, "break");
    add_keyword(Token::Continue, "continue");
    add_keyword(Token::Return, "return");
    
    add_define(Token::Pre_If, "#if");
    add_define(Token::Pre_IfDef, "#ifdef");
    add_define(Token::Pre_ElseDef, "#else");
    add_define(Token::Pre_Elif, "#elif");
    add_define(Token::Pre_End, "#end");
    add_define(Token::Pre_Include, "#include");
}

Token lex_identifier(Lexer& lex) {
    uint start = lex.i-1;
    char c = last(lex);
    //assert(is_alpha(c));
    
    while (char c = peek(lex)) {
        if (!is_alpha_numeric(c)) break;
        adv(lex);
    }
    
    Token token;
    string_view str = {lex.src.data + start, lex.i-start};
    
    if (lookup_match(keywords, lex, str, token)) return token;
     
    token = make_token(lex, Token::Identifier, str.length);
    token.value_str = str;
    
    return token;
}

Token lex_define(Lexer& lex) {
    uint start = lex.i;
    
    while (char c = peek(lex)) {
        if (!is_alpha(c)) break;
        adv(lex);
    }
    
    Token token;
    string_view str = {lex.src.data + start, lex.i-start};
    
    if (lookup_match(keywords, lex, str, token)) return token;
    
    return make_token(lex, Token::Pre_If, str.length+1);
}

Token lex_string(Lexer& lex) {
    uint start = lex.i-1;
    
    bool escape = false;
    while (char c = next(lex)) {
        if (c == '\\') escape = true;
        else if (c == '"' && !escape) break;
        else escape = false;
    }
    
    string_view str = {lex.src.data + start, lex.i-start};
    Token token = make_token(lex, Token::String, str.length);
    token.value_str = str;
    
    return token;
}

Token op_or_assign(Lexer& lex, Token::Type op) {
    if (peek(lex) == '=') {
        adv(lex);
        return make_token(lex, Token::Type(Token::Assign_Begin + op - Token::Op_Begin), 2);
    }
    else {
        return make_token(lex, op, 1);
    }
}

Token lex_token(Lexer& lex) {
    char c = next(lex);
    
    if (c == '\0') return make_token(lex, Token::End_Of_File, 0);
    if (is_digit(c)) return lex_number(lex);
    if (is_alpha(c)) return lex_identifier(lex);
    
    switch (c) {
        case '#': return lex_define(lex);
        case '"': return lex_string(lex);
        case '(': return make_token(lex, Token::Open_Paren, 1);
        case ')': return make_token(lex, Token::Close_Paren, 1);
        case '{': return make_token(lex, Token::Open_Bracket, 1);
        case '}': return make_token(lex, Token::Close_Bracket, 1);
        case '[': return make_token(lex, Token::Open_Square, 1);
        case ']': return make_token(lex, Token::Close_Square, 1);
        case '=': return op_or_assign(lex, Token::Assign);
        case '+': return op_or_assign(lex, Token::Op_Add);
        case '-': return op_or_assign(lex, Token::Op_Sub);
        case '*': return op_or_assign(lex, Token::Op_Mul);
        case '/': return op_or_assign(lex, Token::Op_Div);
        case '%': return op_or_assign(lex, Token::Op_Mod);
        case '<': return op_or_assign(lex, Token::Op_Lt);
        case '>': return op_or_assign(lex, Token::Op_Gt);
        case ':': return make_token(lex, Token::Colon, 1);
        case ';': return make_token(lex, Token::Semicolon, 1);
        case ',': return make_token(lex, Token::Comma, 1);
        default:
            //printf("ERRO!!\n");
            return {};
    }
}

slice<Token> lex_src(Lexer& lex, string_view src) {
    lex.tokens.clear();
    lex.src = src;
    lex.i = 0;
    lex.column = 0;
    lex.line = 0;
    
    char c;
    while ((c = peek(lex))) {
        if (c == '\n') {
            adv(lex);
            lex.line++;
            lex.column = 0;
        }
        else if (c == ' ') adv(lex);
        else {
            Token token = lex_token(lex);
            lex.tokens.append(token);
        }
    }
    
    lex.tokens.append(make_token(lex, Token::End_Of_File, 1));
    
    return lex.tokens;
}
