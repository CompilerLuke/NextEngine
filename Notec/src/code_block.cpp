//
//  renderer.cpp
//  Notec
//
//  Created by Antonella Calvia on 28/02/2021.
//

#include <stdio.h>

#include <float.h>
#include "engine/input.h"

#include "ui/ui.h"
#include "ui/layout.h"
#include "ui/draw.h"

#include "code_block.h"
#include "gap.h"
#include "lexer.h"

CodeBlockView code_block(UI& ui, CodeBlockState* state) {
    CodeBlockView& view = make_element<CodeBlockView>(ui);
    view.state = state;
    
    return view;
}

struct CodeBlockLayedOutView : LayedOutUIView {
    Font* font;
    glm::vec2 font_scale;
    uint line_height;
    
    CodeBlockState* state;
    
    bool render(UI&, LayedOutUIView& parent);
    void loose_focus(UI&);
};

LayedOutUIView& CodeBlockView::compute_layout(UI &ui, const BoxConstraint& constraint) {
    CodeBlockState& state = *this->state;
    uint num_lines = state.lines;
    
    float font_size;
    glm::vec2 font_scale;
    Font& font = query_font(ui, &font_size, &font_scale, text_style);

    float line_height = 2 * font.chars['X'].size.y * font_scale.y;
    
    float padding[4]; float margin[4];
    BoxConstraint inner = compute_inner(ui, layout, constraint, padding, margin);
    
    glm::vec2 content;
    content.x = inner.max.x;
    content.y = line_height * num_lines;
    
    CodeBlockLayedOutView& result = alloc_layed_out_view<CodeBlockLayedOutView>(ui, this);
    result.geo = compute_geometry(ui, layout, content, inner, constraint, padding, margin);
    
    result.font = &font;
    result.font_scale = font_scale;
    result.line_height = line_height;
    result.state = this->state;
    
    return result;
}

uint find_line(GapBuffer& buffer, uint target, uint current_line, uint start) {
    if (target == current_line) return start;
    
    int dir = target > current_line ? 1 : -1;
    
    for (int i = start; i >= 0 && i < buffer.length; i += dir) {
        if (buffer.buffer[i] == '\n') {
            current_line += dir;
            if (current_line == target) return i;
        }
    }
    
    return dir==1 ? buffer.length : 0;
}

string_view copy_temporary_contigous_chunk(GapBuffer& buffer, uint begin, uint end) {
    if (buffer.gap_end < begin || buffer.gap_begin > end) {
        return {buffer.buffer + begin, end-begin};
    }
    
    uint len = (end-begin) - (buffer.gap_end-buffer.gap_begin);
    char* contigous = TEMPORARY_ARRAY(char, len+1);

    memcpy(contigous, buffer.buffer + begin, buffer.gap_begin-begin);
    memcpy(contigous+(buffer.gap_begin-begin), buffer.buffer + buffer.gap_end, end-buffer.gap_end);
    
    contigous[len] = '\0';
    
    return {contigous, len};
}

void execute(string_view str);

void insert_tab(CodeBlockState& state) {
    for (uint i = 0; i < 4; i++) insert_char(state.buffer, ' ');
}

void indent(CodeBlockState& state) {
    
}

void insert_newline(CodeBlockState& state) {
    insert_char(state.buffer, '\n');
    state.cursor_line++;
    state.lines++;
    
    char last = state.buffer.buffer[max(0,(int)state.buffer.gap_begin-2)];
    if (last == '{') {
        insert_tab(state);
        int cursor = state.buffer.gap_begin;
        insert_newline(state);
        insert_char(state.buffer, '}');
        move_cursor(state.buffer, cursor-state.buffer.gap_begin);
    }
}

uint determine_column(GapBuffer& buffer, uint start) {
    if (start >= buffer.length) return 0;
        
    uint column = 0;
    for (int i = start; i >= 0; i--) {
        if (i >= buffer.gap_begin && i < buffer.gap_end) i = buffer.gap_begin;
        else if (buffer.buffer[i] == '\n') break;
        else column++;
    }
    return column;
}

uint determine_line_end(GapBuffer& buffer, uint start, uint max = 0) {
    if (max == 0 || max > buffer.length) max = buffer.length;

    for (int i = start; i < max; i++) {
        if (i >= buffer.gap_begin && i < buffer.gap_end) i = buffer.gap_end-1;
        else if (buffer.buffer[i] == '\n') return i;
    }
    return max;
}

bool handle_key_input(CodeBlockState& state, Input& input) {
    bool edit = false;
    if (input.key_pressed(Key::Backspace)) {
        state.cursor_column--;
        remove_char(state.buffer);
        return true;
    }
    
    GapBuffer& buffer = state.buffer;
    
    if (input.key_pressed(Key::Enter, ModKeys::Control)) {
        execute(copy_temporary_contigous_chunk(state.buffer, 0, state.buffer.length));
    }
    
    if (input.key_pressed(Key::Left)) {
        move_cursor(state.buffer, -1);
    }
    
    if (input.key_pressed(Key::Right)) {
        move_cursor(state.buffer, 1);
    }
    
    if (input.key_pressed(Key::L, ModKeys::Control)) {
        uint column = determine_column(buffer, buffer.gap_begin-1);
        uint end = determine_line_end(buffer, buffer.gap_end);
        
        buffer.gap_begin -= column;        
        buffer.gap_end = end;
        if (buffer.gap_end < buffer.length) buffer.gap_end++;
    }
    
    if (input.key_pressed(Key::M, ModKeys::Control) && buffer.gap_begin>0) {
        char c = buffer.buffer[buffer.gap_begin-1];
        char opp = '\0';
        int dir = 0;
        
        if (c == '{') opp = '}', dir = 1;
        if (c == '}') opp = '{', dir = -1;
        
        if (c == '(') opp = ')', dir = 1;
        if (c == ')') opp = '(', dir = -1;
        
        if (c == '[') opp = ']', dir = 1;
        if (c == ']') opp = '[', dir = -1;
        
        int count = 0;
            
        if (opp) {
            int i = buffer.gap_begin-1;
            for (; i >= 0 && i < buffer.length; i += dir) {
                if (i >= buffer.gap_begin && i < buffer.gap_end) i = (dir==1 ? buffer.gap_end-1 : buffer.gap_begin);
                else if (buffer.buffer[i] == c) count++;
                else if (buffer.buffer[i] == opp && --count == 0) break;
            }
            
            if (i != buffer.gap_begin) {
                move_cursor(buffer, dir==1 ? i+1-buffer.gap_end : i-buffer.gap_begin+1);
            }
        }
    }
    
    if (input.key_pressed(Key::Up)) {
        int cursor = buffer.gap_begin-1;
        int target = determine_column(buffer, cursor);
        
        if (cursor-target > 0) {
            uint column = determine_column(buffer, cursor - target - 1);
            move_cursor(state.buffer, -target - 1 - max(column - target,0));
        }
    }
    
    if (input.key_pressed(Key::Down)) {
        int cursor = buffer.gap_begin;
        int column = determine_column(buffer, cursor);
        int newline = determine_line_end(buffer, cursor)+1;
        
        if (newline < buffer.length) {
            int target = determine_line_end(buffer, newline, newline+column); //ensures that we do not move past the line
            move_cursor(state.buffer, target - cursor - (buffer.gap_end-buffer.gap_begin));
        }
    }
    
    uint c = input.last_key;

    //if (input.key_down(Key::Y, ModKeys::Control)) app.font_size *= 1 + dt;
    //if (input.key_down(Key::X, ModKeys::Control)) app.font_size /= 1 + dt;
    
    if (input.key_pressed(Key::Enter)) insert_newline(state);
    if (input.key_pressed(Key::Tab)) insert_tab(state);
    
    if (c != 0 && c < 256) {
        state.cursor_column++;
        edit = true;
        insert_char(state.buffer, c);
    }
    
    return true;
}

bool CodeBlockLayedOutView::render(UI& ui, LayedOutUIView& parent) {
    to_absolute_position(geo, parent);
    
    UICmdBuffer& cmd_buffer = current_layer(ui);
    
    Rect2D rect = cmd_buffer.clip_rect_stack.last();
    
    float yoffset = rect.pos.y - geo.inner.pos.y;
    
    uint line_begin = yoffset / line_height;
    uint line_end = line_begin + ceilf(rect.size.y / line_height);
    
    CodeBlockState& state = *this->state;
    
    GapBuffer& buffer = state.buffer;

    uint begin = 0;
    //find_line(buffer, line_begin, state.visible_line_begin, state.visible_begin);
    uint end = buffer.length;
    //find_line(buffer, line_end, state.visible_line_end, state.visible_end);
    
    string_view src = copy_temporary_contigous_chunk(buffer, begin, end);
    
    auto tokens = lex_src(*state.lexer, src);
    uint curr = 0;
    
    
    Input& input = get_input(ui);
    
    bool active = true;
    if (active) {
        handle_key_input(state, input);
    }
    
    float line_number_width = 50*font_scale.x;
    float margin = 100*font_scale.x;
    
    glm::vec2 pos = geo.inner.pos;
    pos.x += margin;
    
    uint line = 1;
    
    auto draw_line_number = [&](uint n) {
        glm::vec4 line_color = color4(150,150,150);
        
        char buffer[10];
        sprintf(buffer, "%i", n);
        
        FontInfo info{this->font, this->font_scale, line_color};
        glm::vec2 size = calc_size_of(info, buffer);
        
        //remove hacky constant
        draw_text(cmd_buffer, buffer, pos + glm::vec2(-margin + (line_number_width-size.x), (0.9/2.0)*line_height), info);
    };
    
    auto draw_cursor = [&](uint i) {
        if (i == state.buffer.gap_begin) { //cursor
            draw_quad(cmd_buffer, pos + glm::vec2(0,(0.9/2.0)*line_height), glm::vec2(2.0*font_scale.x,12), white);
        }
    };
    
    draw_line_number(1);
    
    uint i = 0;
    for (; i < src.length; i++) {
        while (tokens[curr].loc<i) {
            if (curr+1 < tokens.length) curr++;
            else break;
        }
        
        Token& token = tokens[curr];
        
        Color color = white;
        if (Token::Keyword_Begin <= token.type && token.type <= Token::Keyword_End) {
            color = color4(255,0,100);
        }
        if (Token::Number_Begin <= token.type && token.type <= Token::Number_End) {
            color = color4(255,230,0);
        }
        else if (Token::Preprocessor_Begin <= token.type && token.type <= Token::Preprocessor_End) {
            color = color4(255, 200, 100);
        }
        else if (token.type == Token::Identifier) {
            color = color4(255,255,255);
        }
        else if (token.type == Token::String) {
            color = color4(255, 150, 100);
        }
        
        if (src[i] == '\n') {
            draw_cursor(i);
            pos.x = geo.inner.pos.x + margin;
            pos.y += line_height;
            
            draw_line_number(++line);
            
            continue;
        }
        
        Character ch = font->chars[src[i]];
        
        float xpos = pos.x + ch.bearing.x * font_scale.x;
        float ypos = pos.y + (ch.size.y - ch.bearing.y) * font_scale.y;
        
        float w = ch.size.x * font_scale.x;
        float h = ch.size.y * font_scale.y;
        float advance = (ch.advance >> 6) * font_scale.x;
        
        draw_cursor(i);
        draw_quad(cmd_buffer, glm::vec2(xpos, ypos - h + line_height), glm::vec2(w, h), font->atlas, color, ch.a, ch.b);
        pos.x += advance;
    }
    draw_cursor(i);
    
    return false;
}

void CodeBlockLayedOutView::loose_focus(UI& ui) {
    
}

void render() {
    
    /*
    begin_hstack(ui);
    uint line = 0;
    for (Token& token : tokens) {
        const char* type_to_str[Token::Count] = {};
        type_to_str[Token::Op_Add] = "+";
        type_to_str[Token::Op_Sub] = "-";
        type_to_str[Token::Op_Mul] = "*";
        type_to_str[Token::Op_Div] = "/";
        type_to_str[Token::Op_Mod] = "%";
        type_to_str[Token::Assign_Add] = "+=";
        type_to_str[Token::Assign_Sub] = "-=";
        type_to_str[Token::Assign_Mul] = "*=";
        type_to_str[Token::Assign_Div] = "/=";
        type_to_str[Token::Assign_Mod] = "%=";
        
        type_to_str[Token::Open_Paren] = "(";
        type_to_str[Token::Close_Paren] = ")";
        type_to_str[Token::Open_Bracket] = "{";
        type_to_str[Token::Close_Bracket] = "}";
        
        type_to_str[Token::While] = "while";
        type_to_str[Token::For] = "for";
        type_to_str[Token::Break] = "break";
        type_to_str[Token::Continue] = "continue";
        
        type_to_str[Token::If] = "if";
        type_to_str[Token::Elif] = "elif";
        type_to_str[Token::Else] = "else";
        type_to_str[Token::True] = "true";
        type_to_str[Token::False] = "false";
        
        type_to_str[Token::Struct] = "struct";
        type_to_str[Token::Def] = "def";
        
        type_to_str[Token::Semicolon] = ";";
        type_to_str[Token::Colon] = ":";
        
        char* str = TEMPORARY_ARRAY(char, 100);
        
        Color color = white;
        if (Token::Keyword_Begin <= token.type && token.type <= Token::Keyword_End) {
            color = color4(255,0,100);
        }
        if (Token::Number_Begin <= token.type && token.type <= Token::Number_End) {
            color = color4(255,255,0);
            if (token.type <= Token::I64) {
                snprintf(str, 100, "%lld", token.value_int);
            } else {
                snprintf(str, 100, "%g", token.value_float);
            }
        }
        else if (token.type == Token::Identifier) {
            uint len = min(token.value_str.length, 99);
            memcpy(str, token.value_str.data, len);
            str[len] = '\0';
        }
        else {
            const char* src = type_to_str[token.type];
            if (src) strcpy(str, type_to_str[token.type]);
            else strcpy(str, "X");
        }
        
        text(ui, str)
            .font(app.font_size)
            .padding(5)
            .color(color);
        
        while (token.loc.line != line) {
            line++;
            end_hstack(ui);
            begin_hstack(ui);
        }
    }
    end_hstack(ui);
    */
}
