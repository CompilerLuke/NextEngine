#pragma once

#include "core/core.h"
#include "ui/ui.h"
#include "gap.h"

struct Lexer;

struct CodeBlockState {
    Lexer* lexer;
    
    GapBuffer buffer;
    
    uint lines = 1;
    
    uint cursor_line;
    uint cursor_column;
    
    uint visible_line_begin;
    uint visible_line_end;
    uint visible_begin;
    uint visible_end;
};

struct CodeBlockView : UIView {
    LayoutStyle layout_style;
    TextStyle text_style;
    
    CodeBlockState* state;
    
    LAYOUT_PROPERTIES(CodeBlockView)
    TEXT_PROPERTIES(CodeBlockView)
    
    LayedOutUIView& compute_layout(UI& ui, const BoxConstraint&) override;
};

CodeBlockView code_block(UI& ui, CodeBlockState* state);
