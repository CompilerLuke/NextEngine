#pragma once

#include "core/container/string_view.h"
#include "core/container/tvector.h"
#include "graphics/rhi/draw.h"
#include <glm/vec2.hpp>
#include "UI/event.h"
#include "UI/font.h"
#include "graphics/rhi/window.h"

struct UI;
struct Input;
struct UIDrawData;
struct CommandBuffer;
struct LayedOutUIView;

enum SizeMode { Px, Perc, Grow };

struct Size {
    SizeMode mode;
    float value;
    
    Size() : mode(Px), value(0) {}
    Size(float value) : mode(Px), value(value) {}
    Size(SizeMode mode, float value) : mode(mode), value(value) {}
    
    bool operator==(Size& other) {
        return mode==other.mode && value==other.value;
    }
};

enum PositionMode {RelativePx, AbsolutePx, AbsolutePerc, RelativePerc };

struct Position {
    PositionMode mode = RelativePx;
    glm::vec2 value;
    
    Position() : mode(RelativePx), value(0) {}
    Position(glm::vec2 value) : mode(RelativePx), value(value) {}
    Position(float x, float y) : mode(RelativePx), value(glm::vec2(x,y)) {}
    Position(PositionMode mode, glm::vec2 value) : mode(mode), value(value) {}
    Position(PositionMode mode, float x, float y) : mode(mode), value(glm::vec2(x,y)) {}
};

enum VAlignment { VTop, VCenter, VBottom };
enum HAlignment { HLeading, HCenter, HTrailing };

enum EdgeAlignment {
    ELeading, ETrailing, ETop, EBottom
};

enum TextAlignment {
    TLeading, TCenter, TTrailing, TJustify
};

struct BoxConstraint {
    glm::vec2 min = glm::vec2(0);
    glm::vec2 max = glm::vec2(FLT_MAX);
};

using Color = glm::vec4;
constexpr Color BLACK = glm::vec4(0,0,0,1);
constexpr Color WHITE = glm::vec4(1,1,1,1);

constexpr Color color4(uint r, uint g, uint b, uint a = 1.0) {
    return glm::vec4(r/255.0f,g/255.0f,b/255.0f,a);
}


//using LayoutFlags = uint;
//const LayoutFlags FLOATING_VIEW = 1 << 0;

struct LayoutStyle {
    VAlignment valignment = VCenter;
    HAlignment halignment = HCenter;
    Size padding[4];
    Size margin[4];
    Size min_width;
    Size min_height;
    Size max_width = {Grow,0};
    Size max_height = {Grow,0};
    Position position;
    
    glm::vec2 flex_factor = glm::vec2(0.0f);
    
    void set_padding(Size padding, EdgeAlignment align) {
        this->padding[(uint)align] = padding;
    }
    
    void set_padding(Size padding) {
        for (uint i = 0; i < 4; i++) this->padding[i] = padding;
    }
    
    void set_margin(Size margin, EdgeAlignment align) {
        this->margin[(uint)align] = margin;
    }
    
    void set_margin(Size margin) {
        for (uint i = 0; i < 4; i++) this->margin[i] = margin;
    }
    
    void set_flex(glm::vec2 factor) {
        flex_factor = factor;
    }
};

#define LAYOUT_PROPERTIES(T) \
T& padding(EdgeAlignment align, Size padding) { layout.set_padding(padding, align); return *this; } \
T& padding(Size padding) { layout.set_padding(padding); return *this; }\
T& margin(EdgeAlignment align, Size margin) { layout.set_margin(margin, align); return *this; }\
T& margin(Size margin) { layout.set_margin(margin); return *this; } \
T& flex(glm::vec2 factor) { layout.set_flex(factor); return *this; } \
T& frame(Size width, Size height) { layout.min_width = width; layout.min_height = height; layout.max_width = width; layout.max_height = height; return *this; } \
T& align(VAlignment alignment) { layout.valignment = alignment; return *this; }\
T& align(HAlignment alignment) { layout.halignment = alignment; return *this; }\
T& width(Size width) { layout.min_width = width; layout.max_width = width; return *this; } \
T& width(Size min, Size max) { layout.min_width = min; layout.max_width = max; return *this; } \
T& height(Size height) { layout.min_height = height; layout.max_height = height; return *this; } \
T& height(Size min, Size max) { layout.min_height = min; layout.max_height = max; return *this; } \
T& background(Color color) { bg.color = color; return *this; } \
T& offset(Position position) { layout.position = position; return *this; } \
T& border(Size thickness, Color color) { \
    for (uint i = 0; i < 4; i++) bg.border_thickness[i] = thickness; \
    bg.border_color = color; \
    return *this; \
} \
T& border(EdgeAlignment edge, Size thickness, Color color) { \
    bg.border_thickness[edge] = thickness; \
    bg.border_color = color; \
    return *this; \
}

struct TextStyle {
    font_handle font;
    Size font_size = {Px, 12};
    Size line_spacing = {Px, 0};
    Color color = BLACK;
    uint line_limit = 1;
    TextAlignment alignment = TJustify;
};

#define TEXT_PROPERTIES(T) \
T& font(font_handle font) { text_style.font = font; return *this; } \
T& font(Size size, font_handle font = {0}) { text_style.font_size = size; if (font.id) text_style.font = font; return *this; }\
T& line_limit(uint limit) { text_style.line_limit = limit; if (limit > 1) { layout.flex_factor.x = 1.0; } return *this; } \
T& color(Color color) { text_style.color = color; return *this; }

struct Events {
    Message on_click;
    Message on_double_click;
    Message on_hover;
    Message on_focus;
    Message on_loose_focus;
};

#define EVENT_PROPERTIES(T) \
T& on_click(Message mesg) { events.on_click = mesg; return *this; } \
T& on_double_click(Message mesg) { events.on_double_click = mesg; return *this; } \
T& on_hover(Message mesg) { events.on_hover = mesg; return *this; } \
T& on_focus(Message mesg) { events.on_focus = mesg; return *this; } \
T& on_loose_focus(Message mesg) { events.on_loose_focus = mesg; return *this; }

struct BgStyle {
    Color color;
    Size border_thickness[4];
    Color border_color;
};

struct UIView {
    LayoutStyle layout;
    Events events;
    BgStyle bg;
    virtual LayedOutUIView& compute_layout(UI& ui, const BoxConstraint&) = 0;
};

struct UIContainer : UIView {
    tvector<UIView*> children;
};

struct Text : UIView {
    TextStyle text_style;
    string_view text;
    
    LAYOUT_PROPERTIES(Text)
    TEXT_PROPERTIES(Text)
    EVENT_PROPERTIES(Text)
    
    LayedOutUIView& compute_layout(UI& ui, const BoxConstraint&) override;
};

struct Button : UIView {
    TextStyle text_style;
    string_view text;
    
    LAYOUT_PROPERTIES(Button)
    TEXT_PROPERTIES(Button)
    EVENT_PROPERTIES(Button)
    
    LayedOutUIView& compute_layout(UI& ui, const BoxConstraint&) override;
};

enum ImageScale {
    Frame, //todo: does this mode make sense to have
    Fill,
    Fit,
    Fixed
};

constexpr float KEEP_ASPECT = 0.0;
constexpr float IGNORE_ASPECT = -1.0;

struct ImageView : UIView {
    texture_handle texture;
    
    ImageScale image_scale = Fixed;
    float aspect_ratio = IGNORE_ASPECT;
    glm::vec2 a = glm::vec2(0,0);
    glm::vec2 b = glm::vec2(1,1);
    glm::vec4 image_color = WHITE;
    
    LAYOUT_PROPERTIES(ImageView)
    
    ImageView& color(Color color) {
        image_color = color;
        return *this;
    }
    
    ImageView& resizeable() {
        image_scale = Frame;
        layout.flex_factor = glm::vec2(1.0);
        return *this;
    }
    
    ImageView& scale_to_fit() {
        aspect_ratio = KEEP_ASPECT;
        image_scale = Fit;
        layout.flex_factor = glm::vec2(1.0);
        return *this;
    }
    
    ImageView& scale_to_fill() {
        aspect_ratio = KEEP_ASPECT;
        image_scale = Fill;
        layout.flex_factor = glm::vec2(1.0);
        return *this;
    }
    
    ImageView& aspect(float ratio) {
        aspect_ratio = ratio;
        return *this;
    }
    
    ImageView& aspect(float ratio, ImageScale scale) {
        aspect_ratio = ratio;
        image_scale = scale;
        return *this;
    }
    
    ImageView& crop(glm::vec2 pos, glm::vec2 size) {
        glm::vec2 d = b-a;
        a += d*pos;
        b = a + size*d;
        return *this;
    }
    
    ImageView& flip(bool vertical = true) {
        std::swap(a[vertical], b[vertical]);
        return *this;
    }
    
    LayedOutUIView& compute_layout(UI& ui, const BoxConstraint&) override;
};

struct Spacer : UIView {
    LAYOUT_PROPERTIES(Spacer)

    LayedOutUIView& compute_layout(UI& ui, const BoxConstraint&) override;
};

struct StackView : UIContainer {
    Size spacing;
    uint alignment;
    bool axis;
    
    LAYOUT_PROPERTIES(StackView)
    EVENT_PROPERTIES(StackView)
    
    LayedOutUIView& compute_layout(UI& ui, const BoxConstraint&) override;
};


struct Hash {
    u64 hash;
    
    Hash(const char* str) : hash(hash_func(str)) {}
    Hash(string_view str) : hash(hash_func(str)) {}
    Hash(u64 hash) : hash(hash) {}
};

struct ScrollView : UIContainer {
    u64 hash;
    Color scroll_bar_color;
    
    LAYOUT_PROPERTIES(ScrollView)
    EVENT_PROPERTIES(ScrollView)
    
    LayedOutUIView& compute_layout(UI& ui, const BoxConstraint&) override;
};

using PanelFlags = uint;
constexpr PanelFlags PANEL_MOVABLE = 1 << 0;
constexpr PanelFlags PANEL_RESIZEABLE_BASE = 1 << 0;

struct PanelView : UIContainer {
    u64 hash;
    struct PanelState* state;
    PanelFlags flags;
    
    LAYOUT_PROPERTIES(PanelView)
    
    LayedOutUIView& compute_layout(UI& ui, const BoxConstraint&) override;
    
    inline PanelView& resizeable(EdgeAlignment edge) {
        flags |= PANEL_RESIZEABLE_BASE << edge;
        return *this;
    }
    
    inline PanelView& resizeable() {
        for (uint i = 0; i < 4; i++) flags |= PANEL_RESIZEABLE_BASE << i;
        return *this;
    }
    
    PanelView& movable() {
        flags |= PANEL_MOVABLE;
        return *this;
    }
    
    PanelView& open(bool);
    bool is_open();
};

struct SplitterView : UIContainer {
    u64 hash;
    Color splitter_color;
    Color splitter_hover_color;
    Size splitter_thickness;
    bool axis;
    
    LAYOUT_PROPERTIES(SplitterView)
    
    inline SplitterView& color(Color color) {
        splitter_color = color;
        return *this;
    }
    
    inline SplitterView& thickness(Size thickness) {
        splitter_thickness = thickness;
        return *this;
    }
    
    LayedOutUIView& compute_layout(UI& ui, const BoxConstraint&) override;
};

struct InputString {
    string_view placeholder;
    char* buffer;
    uint len;
    Color cursor;
    Color placeholder_color;
};

struct InputStringView : UIContainer {
    TextStyle text_style;
    InputString input;
    
    LAYOUT_PROPERTIES(InputStringView)
    TEXT_PROPERTIES(InputStringView)
    
    LayedOutUIView& compute_layout(UI& ui, const BoxConstraint&) override;
};

struct InputInt {
    int* value;
    int inc;
    int min;
    int max;
    int px_per_inc;
    Color cursor;
    Color hover;
};

struct InputIntView : UIContainer {
    TextStyle text_style;
    InputInt input;
    
    LAYOUT_PROPERTIES(InputIntView)
    TEXT_PROPERTIES(InputIntView)
    
    LayedOutUIView& compute_layout(UI& ui, const BoxConstraint&) override;
};

struct InputFloat {
    float* value;
    float inc;
    float min;
    float max;
    float px_per_inc;
    Color cursor;
    Color hover;
};

struct InputFloatView : UIContainer {
    TextStyle text_style;
    InputFloat input;
    
    LAYOUT_PROPERTIES(InputFloatView)
    TEXT_PROPERTIES(InputFloatView)
    
    LayedOutUIView& compute_layout(UI& ui, const BoxConstraint&) override;
};

UI* make_UI();
void destroy_UI(UI* ui);

void set_default_font(UI& ui, font_handle font);

struct ScreenInfo {
    uint window_width;
    uint window_height;
    uint fb_width;
    uint fb_height;
    uint dpi_h;
    uint dpi_v;
};

enum class ThemeColor {
    Button,
    ButtonHover,
    Text,
    Panel,
    PanelBorder,
    PanelResizeHover,
    Divider,
    Splitter,
    SplitterHover,
    Cursor,
    Placeholder,
    Input,
    InputHover,
    Scrollbar,
    Count
};

enum class ThemeSize {
    DefaultFontSize,
    Title1FontSize,
    Title2FontSize,
    Title3FontSize,
    TextPadding,
    ButtonPadding,
    StackPadding,
    StackMargin,
    StackSpacing,
    InputPadding,
    InputMaxWidth,
    InputMinWidth,
    SplitterThickness,
    VecSpacing,
    Count
};

struct UITheme {
    Color colors[(uint)ThemeColor::Count];
    Size sizes[(uint)ThemeSize::Count];
    
    inline Color color(ThemeColor col) {
        return colors[(uint)col];
    }
    
    inline Size size(ThemeSize col) {
        return sizes[(uint)col];
    }
    
    inline UITheme& color(ThemeColor col, Color value) {
        colors[(uint)col] = value;
        return *this;
    }
    
    inline UITheme& size(ThemeSize size, Size value) {
        sizes[(uint)size] = value;
        return *this;
    }
};

void begin_ui_frame(UI& ui, const ScreenInfo&, const Input&, CursorShape cursor_shape);
void end_ui_frame(UI& ui);
UITheme& get_ui_theme(UI& ui);
UIDrawData& get_ui_draw_data(UI& ui);
CursorShape get_ui_cursor_shape(UI& ui);

font_handle load_font(UI& ui, string_view path);

Text& title1(UI& ui, string_view);
Text& title2(UI& ui, string_view);
Text& title3(UI& ui, string_view);
Text& text(UI& ui, string_view);
Text& button(UI& ui, string_view);
ImageView& image(UI& ui, texture_handle image);
ImageView& image(UI& ui, string_view src);
Spacer& spacer(UI& ui, uint flex = 1);

StackView& begin_vstack(UI& ui, Size spacing = -1.0f, HAlignment alignment = HLeading);
StackView& begin_vstack(UI& ui, HAlignment alignment);
void end_vstack(UI& ui);


StackView& begin_hstack(UI& ui, Size spacing = -1.0f, VAlignment alignment = VCenter);
StackView& begin_hstack(UI& ui, VAlignment alignment);
void end_hstack(UI& ui);

PanelView& begin_panel(UI& ui, Hash hash);
void end_panel(UI& ui);

SplitterView& begin_hsplitter(UI& ui, Hash hash);
SplitterView& begin_vsplitter(UI& ui, Hash hash);
void end_hsplitter(UI& ui);
void end_vsplitter(UI& ui);

void open_panel(UI& ui, Hash hash);
void close_panel(UI& ui, Hash hash);


InputStringView& input(UI& ui, char* buffer, uint len);
InputIntView& input(UI& ui, int* value, int min = -INT_MAX, int max = INT_MAX, int px_per_inc = 10);
InputFloatView& input(UI& ui, float* value, float min = -FLT_MAX, float max = FLT_MAX, float px_per_inc = 1.0);
StackView& input(UI& ui, glm::vec3* vec, float min = -FLT_MAX, float max = FLT_MAX, float px_per_inc = 10.0);

ScrollView& begin_scroll_view(UI& ui, Hash hash);
void end_scroll_view(UI& ui);
