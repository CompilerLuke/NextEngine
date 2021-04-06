#pragma once

#include "core/container/slice.h"

struct Token;
struct AST;
struct AstModule;

struct Parser;

Parser* make_parser();
void destroy_parser(Parser*);

AST* parse_tokens(AstModule&, slice<Token> tokens);
