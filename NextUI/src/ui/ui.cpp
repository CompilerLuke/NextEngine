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

glm::vec4 shade1 = color4(30, 30, 30, 1);
glm::vec4 shade2 = color4(40, 40, 40, 1);
glm::vec4 shade3 = color4(50, 50, 50, 1);
glm::vec4 border = color4(100, 100, 100, 1);
glm::vec4 blue = color4(52, 159, 235, 1);
glm::vec4 white = color4(255, 255, 255, 1);
glm::vec4 black = color4(0, 0, 0, 1);

UI* make_ui(Renderer& renderer) {
    u64 block_size = mb(1);

    UI* ui = PERMANENT_ALLOC(UI);
    ui->renderer = make_ui_renderer(renderer, ui->draw_data);
    ui->curr_allocator = LinearAllocator(block_size, &default_allocator);
    ui->prev_allocator = LinearAllocator(block_size, &default_allocator);
    
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

void debug_print_tree(UI& ui, UIView* base) {
#if 0
    printf("=====");
    auto dfs = [&](auto recur, UIView* view) -> void {
        assert(view->children.allocator == &ui.curr_allocator);
        printf("View [%u] [%u]\n", view->m_id.hash, view->guid);
        for(UIView* child : view->children) {
            recur(recur, child);
        }
    };
    dfs(dfs, base);
#endif
}

UI_GID find_or_create_guid(UI& ui, std::type_index type, UI_GID parent_id, int child_index, UI_ID id) {
    if (id.type == UI_ID::Global) return id.hash;

    UIView* parent = ui.prev_container_by_id[parent_id]; // todo: maybe pointer would be more efficient
    if (parent) {
        for (int k = 0; k < 2; k++) {
            int search_dir = k == 0 ? 1 : -1;
            for (int i = child_index; 0 <= i && i < parent->children.length; i += search_dir) {
                UIView *child = parent->children[i];
                if (child->m_id == id) {
                    child->m_id = {UI_ID::Global, UINT64_MAX}; // Ensure multiple nodes don't use the same id
                    if(child->guid == 0) {
                        debug_print_tree(ui, ui.prev_tree_root);
                    }
                    assert(child->guid != 0);
                    return child->guid;
                }
            }
        }
    }
    //Questionable generation of ids, could generate ids in 0-1000, by incrementing counter, and avoid a hash map lookup
    return rand();
}

UI_GID find_or_create_guid(UI& ui, std::type_index type, UI_ID id) {
    UI_ID parent = ui.container_stack.last()->guid;
    return find_or_create_guid(ui, type, id);
}

//Somewhat awkward as it's finding the stable id for the last view
//as the id property hasn't been set yet
//if the id was a parameter, instead of .id(_)
//this could be avoided
void set_id_of_last_view(UI& ui) {
    auto& cstack = ui.container_stack;
    if (cstack.length == 0) return;
    UIView* container = cstack.last();
    UIView* view = nullptr; UI_GID parent_id = {}; int child_index = 0;
    if(container->children.length == 0 && cstack.length==1) {
        parent_id = 0; // Set id for root node
        child_index = 0;
        view = container;
    }
    else if(container->children.length == 0) {
        parent_id = cstack[cstack.length-2]->guid;
        child_index = cstack[cstack.length-2]->children.length-1;
        view = container;
    } else{
        parent_id = container->guid;
        child_index = container->children.length-1;
        view = container->children.last();
        if(view->children.length > 0) { // Should already have been set by pop_container
            assert(view->guid != 0);
            return;
        }
    }
    view->guid = find_or_create_guid(ui, typeid(*view), parent_id, child_index, view->m_id);
    ui.curr_container_by_id[view->guid] = view;
}

UI_API void* talloc(UI& ui, size_t size) {
    return ui.curr_allocator.allocate(size);
}

void push_element(UI& ui, UIView* view) {
    view->children.allocator = &ui.curr_allocator;
    set_id_of_last_view(ui);
    assert(view->children.length == 0);
    // todo: this is slightly dangerous since curr_allocator and prev_allocator will be swapped, in future have call
    //  get_allocator(ui)
    ui.container_stack.last()->children.append(view);
}

template<typename T>
T& push_container(UI& ui) {
    T& element = make_element<T>(ui);
    ui.container_stack.append(&element);
    return element;
}

void pop_container(UI& ui) {
    if (ui.container_stack.last()->children.length > 0) {
        set_id_of_last_view(ui);
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

StackView& begin_stack(UI& ui, Size spacing, StackView::Axis axis, uint alignment) {
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
    return begin_stack(ui, spacing, StackView::Vertical, alignment);
}

StackView& begin_vstack(UI& ui, HAlignment alignment) {
    return begin_stack(ui, -1, StackView::Vertical, alignment);
}

StackView& begin_hstack(UI& ui, Size spacing, VAlignment alignment) {
    return begin_stack(ui, spacing, StackView::Horizontal, alignment);
}


StackView& begin_hstack(UI& ui, VAlignment alignment) {
    return begin_hstack(ui, -1, alignment);
}

StackView& begin_zstack(UI& ui) {
    return begin_stack(ui, -1, StackView::Depth, 0);
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

void open_panel(UI& ui, UI_GID id) {
    ui.panels[id].open = true;
}

void close_panel(UI& ui, UI_GID id) {
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

void end_zstack(UI& ui) {
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
    input.input.sbuffer = buffer;
    init_input_style(ui, input);
    
    return input;
}

InputStringView& input(UI& ui, sstring* buffer) {
    InputStringView& input = make_element<InputStringView>(ui);
    input.input.type = InputString::SString;
    input.input.ssstring = buffer;
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
    
    StackView* root = alloc_t<StackView>(ui.curr_allocator);
    root->children.allocator = &ui.curr_allocator;
    root->axis = StackView::Vertical;
    root->layout.valignment = VTop;
    root->alignment = HCenter;
    root->frame(ui.draw_data.width_px, ui.draw_data.height_px);
    root->bg.color = glm::vec4(1,1,1,1);
    root->id(UI_ID(UI_ID::Global, "_____ROOT"));
    
    ui.container_stack.append(root);
}

void end_ui_frame(UI& ui) {
    assert_mesg(ui.container_stack.length == 1, "Uneven number of begin and ends");

    UIView* base = ui.container_stack[0];
    pop_container(ui);

    glm::vec2 size(ui.draw_data.width_px, ui.draw_data.height_px);
    
    ui.draw_data.layers.append({});
    ui.draw_data.layers[0].clip_rect_stack.append({{}, size});
    
    LayedOutUIView root = {};
    
    BoxConstraint box{glm::vec2(0), size};

    LayedOutUIView& layed_out = base->compute_layout(ui, box);
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

    ui.prev_tree_root = base;
    ui.container_stack.clear();

    std::swap(ui.prev_allocator, ui.curr_allocator);
    std::swap(ui.prev_container_by_id, ui.curr_container_by_id);
    ui.curr_container_by_id.clear();
    ui.curr_allocator.clear();

    debug_print_tree(ui, base);

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


