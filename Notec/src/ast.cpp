#include "ast.h"
#include <stdlib.h>
#include "core/memory/linear_allocator.h"

#define MALLOC(T, n) (T*)malloc(sizeof(T) * n)
#define CALLOC(T, n) (T*)calloc(n, sizeof(T))

const uint MAX_SYMBOLS_PER_BLOCK = 10000;

struct SymbolTable {
    struct SymbolBlock {
        char names[MAX_SYMBOLS_PER_BLOCK][100];
    };
    
    uint num_blocks;
    SymbolBlock* blocks;
};

struct AstModule {
    AST* blocks;
    AST* free_chain;
    AST* root;
    AstPool* pool;
    AstModule* next;
};

struct AstPool {
    AstModule* free_module;
    AST* free_block;
};

AstPool* make_ast_pool() {
    AstPool* pool = PERMANENT_ALLOC(AstPool, {});
    return pool;
}

void destroy_ast_pool(AstPool*) {
    
}

AstModule* make_ast_module(AstPool* pool) {
    AstModule* code = pool->free_module;
    if (!code) {
        const uint AST_MODULE_BLOCK = 10;
        
        AstModule* block = PERMANENT_ARRAY(AstModule, AST_MODULE_BLOCK);
        memset(block, 0, sizeof(AstModule)*AST_MODULE_BLOCK);
        for (uint i = 2; i < AST_MODULE_BLOCK-1; i++) {
            block[i].next = block[i+1].next;
        }
        code = block+1; //todo avoid wasting one AstModule each time
    } else {
        pool->free_module = code->next;
    }
    return code;
}

AST* get_root(AstModule& module) {
    return module.root;
}

void destroy_ast_module(AstModule* code) {
    while (code->blocks) {
        AST* block = code->blocks;
        block->free_next = code->pool->free_block;
        code->pool->free_block = block;
    }
    code->next = code->pool->free_module;
    code->pool->free_module = code;
}

const uint AST_BLOCK_CHUNK = 1024;

AST* make_ast_node(AstModule& code, AST::Type type) {
    AST* node = code.free_chain;
    if (!node) {
        AST* block = code.pool->free_block;
        if (!block) {
            block = PERMANENT_ARRAY(AST, AST_BLOCK_CHUNK);
            memset(block, 0, sizeof(AST)*AST_BLOCK_CHUNK);
        } else {
            code.pool->free_block = block->free_next;
        }
            
        block->free_next = code.blocks;
        code.blocks = block;
        
        code.free_chain = block + 2;
        for (uint i = 2; i < AST_BLOCK_CHUNK-1; i++) {
            block[i].free_next = block[i+1].free_next;
        }
            
        node = block+1;
    } else {
        code.free_chain = node->free_next;
    }
    
    *node = {type};
    return node;
}

void destroy_ast_node(AstModule& code, AST* ast) {
    ast->free_next = code.free_chain;
    code.free_chain = ast;
}

