#pragma once

#include "core/container/tvector.h"
#include "graphics/rhi/draw.h"
#include "ui.h"

struct Font;
struct UI;

struct UIElementGeo {
    Rect2D extent;
    Rect2D inner;
    Rect2D content;
    Rect2D element;
};

struct FontInfo {
    Font* font;
    glm::vec2 font_scale;
    glm::vec4 font_color;
};

struct TextLayout : FontInfo {
    struct Word {
        glm::vec2 pos;
        string_view text;
    };
    
    tvector<Word> words;
};

struct LayedOutUIView {
    UI_GUID id;
    UIElementGeo geo;
    BgStyle bg;
    Events events;
    virtual bool render(UI&, LayedOutUIView& parent) { return false; }
    virtual void loose_focus(UI&) {}
};

struct LayedOutUIContainer : LayedOutUIView {
    slice<LayedOutUIView*> children;
    
    bool render(UI&, LayedOutUIView& parent) override;
};

struct LayedOutText : LayedOutUIView {
    TextLayout text;
    
    bool render(UI&, LayedOutUIView& parent) override;
};

struct LayedOutImage : LayedOutUIView {
    texture_handle texture;
    glm::vec2 a = glm::vec2(0,0);
    glm::vec2 b = glm::vec2(1,1);
    glm::vec4 image_color;
    
    bool render(UI&, LayedOutUIView& parent) override;
};

struct LayedOutScrollView : LayedOutUIContainer {
    bool render(UI&, LayedOutUIView& parent) override;
};

using PanelFlags = uint;

struct LayedOutPanel : LayedOutUIContainer {
    PanelFlags flags;
    glm::vec2 min_size;
    bool render(UI& ui, LayedOutUIView& parent) override;
};

struct LayedOutSplitter : LayedOutUIView {
    LayedOutUIView* children[2];
    bool axis;
    float thickness;
    glm::vec4 color;
    glm::vec4 hover_color;
    
    bool render(UI& ui, LayedOutUIView& parent) override;
};

struct LayedOutRect : LayedOutUIView {
    glm::vec4 color;
    glm::vec4 stroke;
    
    bool render(UI&, LayedOutUIView& parent) override;
};

struct LayedOutInputString : LayedOutUIView {
    InputString input;
    FontInfo font;
    
    bool render(UI&, LayedOutUIView& parent) override;
    void loose_focus(UI& ui) override;
};

struct LayedOutInputInt : LayedOutUIView {
    InputInt input;
    FontInfo font;
    
    bool render(UI&, LayedOutUIView& parent) override;
    void loose_focus(UI& ui) override;
};

struct LayedOutInputFloat : LayedOutUIView {
    InputFloat input;
    FontInfo font;
    
    bool render(UI&, LayedOutUIView& parent) override;
    void loose_focus(UI& ui) override;
};

inline float gtz(float value) {
    return value > 0 ? value : 0;
}

glm::vec2 calc_size_of(FontInfo& info, string_view text);
void to_size_bounds(float* result, Size size, float max_size);
