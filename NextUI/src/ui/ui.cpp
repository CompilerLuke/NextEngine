//
//  ui.cpp
//  CFD
//
//  Created by Antonella Calvia on 19/12/2020.
//

#include <stdio.h>
#include "ui/internal.h"
#include "ui/ui.h"
#include "ui/draw.h"
#include "ui/layout.h"
#include "graphics/assets/texture.h"
#include "core/memory/linear_allocator.h"
#include "core/container/hash_map.h"
#include "core/container/handle_manager.h"
#include "engine/input.h"
#include "graphics/assets/assets.h"
#include "graphics/rhi/rhi.h"
#include "core/math/vec3.h"

UI* make_ui() {
    UI* ui = PERMANENT_ALLOC(UI);
    ui->renderer = make_ui_renderer();
    ui->allocator = &get_temporary_allocator();
    
    if (FT_Init_FreeType(&ui->font_cache.ft)) {
        printf("Could not initialize FreeType library");
    }
    
    return ui;
}

ScreenInfo get_screen_info(Window& window) {
    int width, height, fb_width, fb_height, dpi_h, dpi_v;

    window.get_window_size(&width, &height);
    window.get_framebuffer_size(&fb_width, &fb_height);
    window.get_dpi(&dpi_h, &dpi_v);

    ScreenInfo screen_info = {};
    screen_info.window_width = width;
    screen_info.window_height = height;
    screen_info.fb_width = fb_width;
    screen_info.fb_height = fb_height;
    screen_info.dpi_h = dpi_h;
    screen_info.dpi_v = dpi_v;
    
    return screen_info;
}

void init_text_style(UI& ui, TextStyle& text_style) {
    text_style.font = ui.default_font;
    text_style.color = ui.theme.color(ThemeColor::Text);
    text_style.font_size = ui.theme.size(ThemeSize::DefaultFontSize);
}


StableID* find_or_create_stable_id(UI& ui, StableID& parent, std::type_index type, UI_ID id) {
    StableAddID& result = ui.id_stack.last();

    uint index = result.children.length;
    if (id.type == UI_ID::Global) {
        result.children.append({ typeid(void) });
        StableID* stable = &ui.stable_id[id.hash];
        stable->id = id.hash;

        return stable;
    }

    for (int i = index; i < parent.children.length; i++) {
        const StableID& child = parent.children[i];
        if (child.hash == id.hash && child.type == type) {
            result.children.append(parent.children[i]);
            parent.children[i] = {};
            return &result.children[index];
        }
    }
    for (int i = min(index, parent.children.length) - 1; i >= 0; i--) {
        const StableID& child = parent.children[i];
        if (child.hash == id.hash && child.type == type) {
            result.children.append(parent.children[i]);
            parent.children[i] = {};
            return &result.children[index];
        }
    }

    //Questionable generation of ids
    //Could generate ids in 0-1000, by incrementing counter
    //And avoid a hash map lookup
    StableID stable;
    stable.id = rand(); // +INT_MAX;
    stable.hash = id.hash;
    stable.type = type;
    result.children.append(stable);
    return &result.children[index];
}

StableID* find_or_create_stable_id(UI& ui, StableID& parent, UIView& view) {
    return find_or_create_stable_id(ui, parent, typeid(view), view._id);
}

//Somewhat awkward as it's finding the stable id for the last view
//as the id property hasn't been set yet
//if the id was a parameter, instead of .id(_)
//this could be avoided
void set_id_of_last_view(UI& ui) {
    auto& cstack = ui.container_stack;
    if (cstack.length == 0) return;
    UIContainer& container = *cstack.last();

    StableAddID& id = ui.id_stack.last();
    if (cstack.length > ui.id_stack.length) {
        StableID* stable = find_or_create_stable_id(ui, *id.ptr, container);

        container.guid = stable->id;
        ui.id_stack.append({ stable });
    }
    else if (container.children.length > 0) {
        UIView& last = *container.children.last();
        StableID* stable = find_or_create_stable_id(ui, *id.ptr, last);
        last.guid = stable->id;
    }
}

UI_GUID get_stable_id(UI& ui, std::type_index type, UI_ID id) {
    StableAddID& stable = ui.id_stack.last();
    return find_or_create_stable_id(ui, *stable.ptr, type, id)->id;
}

void* talloc(UI& ui, size_t size) {
    return ui.allocator->allocate(size);
}

void push_element(UI& ui, UIView* view) {
    set_id_of_last_view(ui);
    ui.container_stack.last()->children.append(view);
}

template<typename T>
T& push_container(UI& ui) {
    set_id_of_last_view(ui);

    T& element = make_element<T>(ui);
    ui.container_stack.append(&element);
    return element;
}

void pop_container(UI& ui) {
    if (ui.container_stack.last()->children.length > 0) {
        set_id_of_last_view(ui);

        StableAddID& stable = ui.id_stack.last();
        stable.ptr->children = stable.children;
        ui.id_stack.pop();
    }

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

GeometryView& begin_geo(UI& ui, std::function<void(glm::vec2,glm::vec2)>&& change) {
    GeometryView& view = push_container<GeometryView>(ui);
    view.change = std::move(change);
    return view;
}

void end_geo(UI& ui) {
    pop_container(ui);
}

StackView& begin_stack(UI& ui, Size spacing, bool axis, uint alignment) {
    if (spacing.value == -1) spacing = ui.theme.size(ThemeSize::StackSpacing);
    
    StackView& stack = push_container<StackView>(ui);
    stack.spacing = spacing;
    stack.alignment = alignment;
    stack.axis = axis;
    stack.margin(ui.theme.size((ThemeSize)((uint)ThemeSize::HStackMargin + axis)));
    stack.padding(ui.theme.size((ThemeSize)((uint)ThemeSize::HStackPadding + axis)));
    
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

ScrollView& begin_scroll_view(UI& ui) {
    ScrollView& scroll = push_container<ScrollView>(ui);
    scroll.flex(glm::vec2(1));
    scroll.scroll_bar_color = ui.theme.color(ThemeColor::Scrollbar);
    return scroll;
}

PanelView& begin_panel(UI& ui) {
    PanelView& panel = push_container<PanelView>(ui);
    panel.background(ui.theme.color(ThemeColor::Panel));
    panel.layout.min_width = 100;
    panel.layout.min_height = 100;
    
    return panel;
}

PanelView& PanelView::open(bool open) {
    //todo implement
    //state->open = open;
    return *this;
}

bool PanelView::is_open() {
    //todo implement
    return true;
}

void open_panel(UI& ui, UI_GUID id) {
    ui.panels[id].open = true;
}

void close_panel(UI& ui, UI_GUID id) {
    ui.panels[id].open = false;
}

void end_panel(UI& ui) {
    pop_container(ui);
}

SplitterView& begin_splitter(UI& ui, bool axis) {
    SplitterView& splitter = push_container<SplitterView>(ui);
    splitter.axis = axis;
    splitter.splitter_color = ui.theme.color(ThemeColor::Splitter);
    splitter.splitter_hover_color = ui.theme.color(ThemeColor::SplitterHover);
    splitter.thickness(ui.theme.size(ThemeSize::SplitterThickness));
    
    return splitter;
}

SplitterView& begin_hsplitter(UI& ui) {
    return begin_splitter(ui, 0);
}

SplitterView& begin_vsplitter(UI& ui) {
    return begin_splitter(ui, 1);
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
    input.input.type = InputString::CString;
    input.input.cstring_buffer = buffer;
    input.input.max = len;
    init_input_style(ui, input);
    
    return input;
}

InputStringView& input(UI& ui, string_buffer* buffer) {
    InputStringView& input = make_element<InputStringView>(ui);
    input.input.type = InputString::StringBuffer;
    input.input.string_buffer = buffer;
    init_input_style(ui, input);
    
    return input;
}

InputStringView& input(UI& ui, sstring* buffer) {
    InputStringView& input = make_element<InputStringView>(ui);
    input.input.type = InputString::SString;
    input.input.sstring = buffer;
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
    StackView& view = begin_hstack(ui, ui.theme.size(ThemeSize::VecSpacing));
    input(ui, &value->x, min, max, px_per_inc);
    input(ui, &value->y, min, max, px_per_inc);
    input(ui, &value->z, min, max, px_per_inc);
    end_hstack(ui);
    return view;
}

StackView& input(UI& ui, glm::vec4* value, float min, float max, float px_per_inc) {
    StackView& view = begin_hstack(ui, ui.theme.size(ThemeSize::VecSpacing));
    input(ui, &value->x, min, max, px_per_inc);
    input(ui, &value->y, min, max, px_per_inc);
    input(ui, &value->z, min, max, px_per_inc);
    input(ui, &value->w, min, max, px_per_inc);
    end_hstack(ui);
    return view;
}

//todo create dedicated type
StackView& divider(UI& ui, Color color) {
    StackView& view = begin_vstack(ui).background(color).width({ Perc,100 }).height(1);
    end_vstack(ui);
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
    
    StackView* root = TEMPORARY_ALLOC(StackView);
    root->axis = 0;
    root->layout.valignment = VTop;
    root->alignment = HCenter;
    root->frame(ui.draw_data.width_px, ui.draw_data.height_px);
    root->bg.color = glm::vec4(1,1,1,1);
    
    ui.container_stack.append(root);
    ui.id_stack.append({ &ui.id_root });
}

void end_ui_frame(UI& ui) {
    assert_mesg(ui.container_stack.length == 1, "Uneven number of begin and ends");

    UIView& base = *ui.container_stack[0];
    pop_container(ui);
    ui.id_stack.clear();

    glm::vec2 size(ui.draw_data.width_px, ui.draw_data.height_px);
    
    ui.draw_data.layers.append({});
    ui.draw_data.layers[0].clip_rect_stack.append({{}, size});
    
    LayedOutUIView root = {};
    
    BoxConstraint box{glm::vec2(0), size};
    

    LayedOutUIView& layed_out = base.compute_layout(ui, box);
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

    if (ui.font_cache.uploading_font_this_frame) {
        end_gpu_upload();
        ui.font_cache.uploading_font_this_frame = false;
    }
}

UITheme& get_ui_theme(UI& ui) { return ui.theme; }
UIDrawData& get_ui_draw_data(UI& ui) { return ui.draw_data; }
CursorShape get_ui_cursor_shape(UI& ui) { return ui.cursor_shape; }

void destroy_UI(UI* ui) {
    FT_Done_FreeType(ui->font_cache.ft);
    destroy_ui_renderer(ui->renderer);
}


