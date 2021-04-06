#pragma once

#include "core/core.h"
#include "lexer.h"
#include "core/container/vector.h"

struct AST;

struct Operator {
    enum Type { Add, Sub, Mul, Div, } type;
    AST* left;
    AST* right;
};

using symbol_handle = uint;

struct Identifier {
    char name[100];
};

struct IntLiteral {
    u64 value;
};

struct Block {
    vector<AST*> children;
};

struct While {
    AST* condition;
    Block block;
};

struct FuncCall {
    AST* function;
    vector<AST*> children;
};

struct Location {};

struct AST {
    enum Type { Operator, Identifier, IntLiteral, If, Elif, Else, While, For, FuncCall } type;
    
    union {
        struct Operator op;
        struct Identifier id;
        struct IntLiteral int_lit;
        AST* free_next;
    };
    
    Location loc;
    AST* parent;
};

struct AstPool;

struct AstModule;

AST* make_ast_node(AstModule&, AST::Type);
AST* make_op_node(AstModule&, Operator::Type, AST* left, AST* right);
AST* get_root(AstModule&);

void destroy_ast_node(AstModule&, AST*);

AstPool* make_ast_pool();
void destroy_ast_pool(AstPool*);

AstModule* make_ast_module(AstPool*);
void destroy_ast_module(AstModule*);
