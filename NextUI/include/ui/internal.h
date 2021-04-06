#pragma once

#include "core/container/hash_map.h"
#include "core/container/tvector.h"
#include "font.h"
#include "ui.h"
#include "draw.h"
#include "engine/input.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <typeindex>

struct LinearAllocator;
struct Input;

typedef struct FT_LibraryRec_  *FT_Library;

struct FontCache {
    FT_Library ft;
    HandleManager<FT_Face, font_handle> fonts;
    hash_map<FontDesc, Font, 103> cached;
    bool uploading_font_this_frame = false;
};

struct StableID {
    std::type_index type = typeid(void);
    UI_GUID id = 0;
    u64 hash;
    vector<StableID> children;
};

struct StableAddID {
    StableID* ptr;
    tvector<StableID> children;
};

struct ScrollState {
    float offset;
};

struct PanelState {
    glm::vec2 position = glm::vec2(0);
    glm::vec2 size = glm::vec2(0);
    bool open = false;
    bool first = true;
    int active_edge = -1;
};

struct SplitterState {
    BoxConstraint last_constraint;
    float splitter_offset;
    bool dragging = false;
    bool first = true;
};

constexpr uint MAX_SCROLL_BARS = 13;
constexpr uint MAX_PANELS = 23;
constexpr uint MAX_SPLITTERS = 13;
constexpr uint MAX_GUIDS = 103;

struct UI {
    FontCache font_cache;
    UIRenderer* renderer;
    LinearAllocator* allocator;
    tvector<UIContainer*> container_stack;
    tvector<StableAddID> id_stack;
    Input input;
    UIDrawData draw_data;
    font_handle default_font;
    CursorShape cursor_shape;
    UITheme theme;
    
    uint dpi_v;
    uint dpi_h;
    
    float max_font_size;
    
    StableID id_root;
    hash_map<u64, StableID, MAX_GUIDS> stable_id;
    hash_map<u64, ScrollState, MAX_SCROLL_BARS> scrolls;
    hash_map<u64, PanelState, MAX_PANELS> panels;
    hash_map<u64, SplitterState, MAX_SPLITTERS> splitters;
    
    char buffer[100];
    UI_GUID active;
    UI_GUID dragging;
    int dragging_edge;
    int cursor_pos;
    LayedOutUIView* active_input;
};
