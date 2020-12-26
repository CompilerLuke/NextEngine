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

struct UIRenderer;
struct CommandBuffer;
struct Viewport;

UIRenderer* make_ui_renderer();
void submit_draw_data(UIRenderer&, CommandBuffer&, UIDrawData& data);
void destroy_ui_renderer(UIRenderer*);
