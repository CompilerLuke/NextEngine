#pragma once

#include "engine/handle.h"
#include "core/container/tvector.h"
#include "core/container/array.h"
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "graphics/rhi/draw.h"

struct UIVertex {
    glm::vec2 pos;
    glm::vec2 uv;
    glm::vec4 col;
};

using UIIndex = uint;

struct UICmd {
    uint index_offset;
    uint count;
    texture_handle texture;
    Rect2D clip_rect;
};

struct UICmdBuffer {
    tvector<UIVertex> vertices;
    tvector<uint> indices;
    tvector<UICmd> cmds;
    
    array<10, Rect2D> clip_rect_stack;
    
    void push_clip_rect(Rect2D);
    void pop_clip_rect();
};


struct UIDrawData {
    array<20, UICmdBuffer> layers;
    uint current;
    
    uint width_px;
    uint height_px;
    
    glm::vec2 px_to_screen;
};

struct UI;
struct UIRenderer;
struct CommandBuffer;
struct Viewport;
struct Font;
struct FontInfo;
struct UIElementGeo;
struct BgStyle;
struct Input;

UIRenderer* make_ui_renderer();
void submit_draw_data(UIRenderer&, CommandBuffer&, UIDrawData& data);
void destroy_ui_renderer(UIRenderer*);

UICmdBuffer& current_layer(UI& ui);

float center(float size_container, float size);
glm::vec2 center(glm::vec2 size_container, glm::vec2 size);

Input& get_input(UI&);

void draw_quad(UICmdBuffer& cmd_buffer, glm::vec2 pos, glm::vec2 size, texture_handle texture, glm::vec4 col = glm::vec4(1), glm::vec2 a = glm::vec2(0,0), glm::vec2 b = glm::vec2(1,1));
void draw_quad(UICmdBuffer& cmd_buffer, glm::vec2 pos, glm::vec2 size, glm::vec4 col);
void draw_quad(UICmdBuffer& cmd_buffer, Rect2D rect, glm::vec4 col);

void draw_text(UICmdBuffer& cmd_buffer, string_view text, glm::vec2 pos, Font& font, glm::vec2 scale, glm::vec4 color);
void draw_text(UICmdBuffer& cmd_buffer, string_view text, glm::vec2 pos, FontInfo& info);

void render_bg(UI& ui, UIElementGeo& geo, const BgStyle& bg);
