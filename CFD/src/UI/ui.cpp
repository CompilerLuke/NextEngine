//
//  ui.cpp
//  CFD
//
//  Created by Antonella Calvia on 19/12/2020.
//

#include <stdio.h>
#include "UI/internal.h"
#include "UI/ui.h"
#include "UI/draw.h"
#include "UI/layout.h"
#include "graphics/assets/texture.h"
#include "core/memory/linear_allocator.h"
#include "core/container/hash_map.h"
#include "core/container/handle_manager.h"
#include "engine/input.h"
#include "graphics/assets/assets.h"
#include "graphics/rhi/rhi.h"

UI* make_UI() {
    UI* ui = PERMANENT_ALLOC(UI);
    ui->renderer = make_ui_renderer();
    ui->allocator = &get_temporary_allocator();
    
    if (FT_Init_FreeType(&ui->font_cache.ft)) {
        printf("Could not initialize FreeType library");
    }
    
    return ui;
}

void init_text_style(UI& ui, TextStyle& text_style) {
    text_style.font = ui.default_font;
    text_style.color = ui.theme.color(ThemeColor::Text);
    text_style.font_size = ui.theme.size(ThemeSize::DefaultFontSize);
}

template<typename T>
T& make_element(UI& ui) {
    T* ptr = (T*)ui.allocator->allocate(sizeof(T));
    new (ptr) T();
    ui.container_stack.last()->children.append(ptr);
    return *ptr;
}

template<typename T>
T& push_container(UI& ui) {
    T& element = make_element<T>(ui);
    ui.container_stack.append(&element);
    return element;
}

void pop_container(UI& ui) {
    assert_mesg(ui.container_stack.length > 0, "Unbalanced push and pop container");
    ui.container_stack.pop();
}

Text& text(UI& ui, string_view str) {
    Text& text = make_element<Text>(ui);
    init_text_style(ui, text.text_style);
    text.text = str;
    text.padding(ui.theme.size(ThemeSize::TextPadding));
    
    return text;
}

Text& button(UI& ui, string_view str) {
    Text& button = make_element<Text>(ui);
    init_text_style(ui, button.text_style);
    button.text = str;
    //ui.theme.size(ThemeSize::TextPadding)
    button.padding(5);
    button.background(ui.theme.color(ThemeColor::Button));
    
    
    return button;
}

Text& title1(UI& ui, string_view str) {
    return text(ui, str).font(ui.theme.size(ThemeSize::Title1FontSize));
}

Text& title2(UI& ui, string_view str) {
    return text(ui, str).font(ui.theme.size(ThemeSize::Title2FontSize));
}

Text& title3(UI& ui, string_view str) {
    return text(ui, str).font(ui.theme.size(ThemeSize::Title3FontSize));
}

ImageView& image(UI& ui, texture_handle texture) {
    ImageView& image = make_element<ImageView>(ui);
    image.texture = texture;
    return image;
}

ImageView& image(UI& ui, string_view filename) {
    return image(ui, load_Texture(filename));
}

StackView& begin_stack(UI& ui, Size spacing, bool axis, uint alignment) {
    if (spacing.value == -1) spacing = ui.theme.size(ThemeSize::StackSpacing);
    
    StackView& stack = push_container<StackView>(ui);
    stack.spacing = spacing;
    stack.alignment = alignment;
    stack.axis = axis;
    stack.margin(ui.theme.size(ThemeSize::StackMargin));
    stack.padding(ui.theme.size(ThemeSize::StackPadding));
    
    return stack;
}

StackView& begin_vstack(UI& ui, Size spacing, HAlignment alignment) {
    return begin_stack(ui, spacing, 1, alignment);
}

StackView& begin_vstack(UI& ui, HAlignment alignment) {
    return begin_stack(ui, -1, 1, alignment);
}

StackView& begin_hstack(UI& ui, Size spacing, VAlignment alignment) {
    return begin_stack(ui, spacing, 0, alignment);
}


StackView& begin_hstack(UI& ui, VAlignment alignment) {
    return begin_hstack(ui, -1, alignment);
}

Spacer& spacer(UI& ui, uint flex) {
    Spacer& spacer = make_element<Spacer>(ui);
    spacer.layout.flex_factor = glm::vec2(flex);
    return spacer;
}

ScrollView& begin_scroll_view(UI& ui, Hash hash) {
    ScrollView& scroll = push_container<ScrollView>(ui);
    scroll.hash = hash.hash;
    scroll.flex(glm::vec2(1));
    scroll.scroll_bar_color = ui.theme.color(ThemeColor::Scrollbar);
    return scroll;
}

PanelView& begin_panel(UI& ui, Hash hash) {
    PanelView& panel = push_container<PanelView>(ui);
    panel.hash = hash.hash;
    panel.state = &ui.panels[hash.hash];
    panel.background(ui.theme.color(ThemeColor::Panel));
    panel.layout.min_width = 100;
    panel.layout.min_height = 100;
    
    return panel;
}

PanelView& PanelView::open(bool open) {
    state->open = open;
    return *this;
}

bool PanelView::is_open() {
    return state->open;
}

void open_panel(UI& ui, Hash hash) {
    ui.panels[hash.hash].open = true;
}

void close_panel(UI& ui, Hash hash) {
    ui.panels[hash.hash].open = false;
}

void end_panel(UI& ui) {
    pop_container(ui);
}

SplitterView& begin_splitter(UI& ui, Hash hash, bool axis) {
    SplitterView& splitter = push_container<SplitterView>(ui);
    splitter.hash = hash.hash;
    splitter.axis = axis;
    splitter.splitter_color = ui.theme.color(ThemeColor::Splitter);
    splitter.splitter_hover_color = ui.theme.color(ThemeColor::SplitterHover);
    splitter.thickness(ui.theme.size(ThemeSize::SplitterThickness));
    
    return splitter;
}

SplitterView& begin_hsplitter(UI& ui, Hash hash) {
    return begin_splitter(ui, hash, 0);
}

SplitterView& begin_vsplitter(UI& ui, Hash hash) {
    return begin_splitter(ui, hash, 1);
}

void end_hsplitter(UI& ui) {
    pop_container(ui);
}

void end_vsplitter(UI& ui) {
    pop_container(ui);
}

void end_vstack(UI& ui) {
    pop_container(ui);
}

void end(UI& ui) {
    pop_container(ui);
}

void end_hstack(UI& ui) {
    pop_container(ui);
}

void end_scroll_view(UI& ui) {
    pop_container(ui);
}

void set_default_font(UI& ui, font_handle font) {
    ui.default_font = font;
}

template<typename T>
void init_input_style(UI& ui, T& view) {
    init_text_style(ui, view.text_style);
    view.width(ui.theme.size(ThemeSize::InputMinWidth), ui.theme.size(ThemeSize::InputMaxWidth));
    view.background(ui.theme.color(ThemeColor::Input));
    view.padding(ui.theme.size(ThemeSize::InputPadding));
    view.input.cursor = ui.theme.color(ThemeColor::Cursor);
}

InputStringView& input(UI& ui, char* buffer, uint len) {
    InputStringView& input = make_element<InputStringView>(ui);
    input.input.buffer = buffer;
    input.input.len = len;
    init_input_style(ui, input);
    
    return input;
}

InputIntView& input(UI& ui, int* value, int min, int max, int px_per_inc) {
    InputIntView& input = make_element<InputIntView>(ui);
    input.input.value = value;
    input.input.inc = 1;
    input.input.min = min;
    input.input.max = max;
    input.input.px_per_inc = px_per_inc;
    input.input.hover = ui.theme.color(ThemeColor::InputHover);
    init_input_style(ui, input);
    
    return input;
}

InputFloatView& input(UI& ui, float* value, float min, float max, float px_per_inc) {
    InputFloatView& input = make_element<InputFloatView>(ui);
    input.input.value = value;
    input.input.inc = 1.0;
    input.input.min = min;
    input.input.max = max;
    input.input.px_per_inc = px_per_inc;
    input.input.hover = ui.theme.color(ThemeColor::InputHover);
    init_input_style(ui, input);
    
    return input;
}

StackView& input(UI& ui, glm::vec3* value, float min, float max, float px_per_inc) {
    StackView& view = begin_hstack(ui, ui.theme.size(ThemeSize::VecSpacing)).padding(0);
    input(ui, &value->x, min, max, px_per_inc);
    input(ui, &value->y, min, max, px_per_inc);
    input(ui, &value->z, min, max, px_per_inc);
    end_hstack(ui);
    return view;
}

void begin_ui_frame(UI& ui, const ScreenInfo& info, const Input& window_input, CursorShape shape) {
    glm::vec2 px_to_screen = glm::vec2(info.dpi_h, info.dpi_v) / 72.0f;
    if (px_to_screen.x < 1) px_to_screen.x = 1.0;
    if (px_to_screen.y < 1) px_to_screen.y = 1.0;
    
    glm::vec2 window_to_px = glm::vec2((float)info.fb_width / info.window_width / px_to_screen.x, (float)info.fb_height / info.window_height / px_to_screen.y);
    
    ui.input = window_input;
    ui.input.mouse_position *= window_to_px;
    ui.input.mouse_offset *= window_to_px;
    ui.input.mouse_offset.y *= -1;
    ui.input.screen_mouse_position *= window_to_px;
    
    //glm::vec2(info.fb_width/info.window_width, info.fb_height/info.window_height);
    
    
    ui.dpi_h = px_to_screen.x * 72;
    ui.dpi_v = px_to_screen.y * 72;
    
    ui.cursor_shape = shape;
    
    ui.draw_data = {};
    ui.draw_data.width_px = ceilf(info.fb_width / px_to_screen.x);
    ui.draw_data.height_px = ceilf(info.fb_height / px_to_screen.y);
    ui.draw_data.px_to_screen = px_to_screen;
    
    ui.max_font_size = 64; //todo make a parameter
    
    StackView* root = TEMPORARY_ALLOC(StackView, {});
    root->axis = 0;
    root->layout.valignment = VTop;
    root->alignment = HCenter;
    root->frame(ui.draw_data.width_px, ui.draw_data.height_px);
    root->bg.color = glm::vec4(1,1,1,1);
    
    ui.container_stack.append(root);
}

void end_ui_frame(UI& ui) {
    assert_mesg(ui.container_stack.length == 1, "Uneven number of begin and ends");
    
    glm::vec2 size(ui.draw_data.width_px, ui.draw_data.height_px);
    
    ui.draw_data.layers.append({});
    ui.draw_data.layers[0].clip_rect_stack.append({{}, size});
    
    LayedOutUIView root = {};
    
    BoxConstraint box{glm::vec2(0), size};
    
    LayedOutUIView& layed_out = ui.container_stack[0]->compute_layout(ui, box);
    bool any_clicked = layed_out.render(ui, root);
    bool clicked = ui.input.mouse_button_pressed(MouseButton::Left);
    bool enter = ui.input.key_pressed(Key::Enter);
    
    if (((!any_clicked && clicked) || enter) && ui.active) {
        ui.active = 0;
        ui.active_input->loose_focus(ui);
    }
    
    if (ui.input.key_pressed(Key::Escape)) {
        ui.active = 0;
    }
    
    
    if (ui.active) {
        ui.cursor_shape = CursorShape::IBeam;
    }
    
    ui.container_stack.clear();
}

UITheme& get_ui_theme(UI& ui) { return ui.theme; }
UIDrawData& get_ui_draw_data(UI& ui) { return ui.draw_data; }
CursorShape get_ui_cursor_shape(UI& ui) { return ui.cursor_shape; }

void destroy_UI(UI* ui) {
    FT_Done_FreeType(ui->font_cache.ft);
    destroy_ui_renderer(ui->renderer);
}


