#include "ui/internal.h"
#include "ui/layout.h"
#include "graphics/assets/assets.h"
#include "engine/input.h"

Input& get_input(UI& ui) {
    return ui.input;
}

UICmdBuffer& current_layer(UI& ui) {
    return ui.draw_data.layers[ui.draw_data.current];
}

void draw_quad(UICmdBuffer& cmd_buffer, glm::vec2 pos, glm::vec2 size, texture_handle texture, glm::vec4 col, glm::vec2 a, glm::vec2 b) {
    
    uint offset = cmd_buffer.vertices.length;
    
    cmd_buffer.vertices.append({glm::vec2(pos.x + size.x, pos.y), glm::vec2(b.x, a.y), col }); //top right
    cmd_buffer.vertices.append({glm::vec2(pos.x + size.x, pos.y + size.y), b, col }); //bottom right
    cmd_buffer.vertices.append({glm::vec2(pos.x, pos.y + size.y), glm::vec2(a.x, b.y), col }); //bottom left
    cmd_buffer.vertices.append({pos, a, col }); //top left
    
    uint index_offset = cmd_buffer.indices.length;
    cmd_buffer.indices.append(offset);
    cmd_buffer.indices.append(offset + 1);
    cmd_buffer.indices.append(offset + 3);
    cmd_buffer.indices.append(offset + 1);
    cmd_buffer.indices.append(offset + 2);
    cmd_buffer.indices.append(offset + 3);

    UICmd cmd;
    cmd.clip_rect = cmd_buffer.clip_rect_stack.last();
    cmd.index_offset = index_offset;
    cmd.texture = texture;
    cmd.count = 6;
    
    cmd_buffer.cmds.append(cmd);
}

void draw_quad(UICmdBuffer& cmd_buffer, glm::vec2 pos, glm::vec2 size, glm::vec4 col) {
    draw_quad(cmd_buffer, pos, size, default_textures.white, col, glm::vec2(0), glm::vec2(0));
}

void draw_quad(UICmdBuffer& cmd_buffer, Rect2D rect, glm::vec4 col) {
    draw_quad(cmd_buffer, rect.pos, rect.size, default_textures.white, col, glm::vec2(0), glm::vec2(0));
}


void draw_text(UICmdBuffer& cmd_buffer, string_view text, glm::vec2 pos, Font& font, glm::vec2 scale, glm::vec4 color) {
    float normal_height = 1.1*font.chars['X'].size.y * scale.y;
    
    for (int i = 0; i < text.length; i++) {
        Character ch = font.chars[text[i]];
        
        float xpos = pos.x + ch.bearing.x * scale.x;
        float ypos = pos.y + (ch.size.y - ch.bearing.y) * scale.y;
        
        float w = ch.size.x * scale.x;
        float h = ch.size.y * scale.y;
        float advance = (ch.advance >> 6) * scale.x;

        draw_quad(cmd_buffer, glm::vec2(xpos, ypos - h + normal_height), glm::vec2(w, h), font.atlas, color, ch.a, ch.b);
        
        pos.x += advance;
    }
}

float center(float size_container, float size) {
    return (size_container - size) * 0.5f;
}

glm::vec2 center(glm::vec2 size_container, glm::vec2 size) {
    return (size_container - size) * 0.5f;
}

void draw_text(UICmdBuffer& cmd_buffer, string_view text, glm::vec2 pos, FontInfo& info) {
    draw_text(cmd_buffer, text, pos, *info.font, info.font_scale, info.font_color);
}

void to_absolute_position(UIElementGeo& geo, LayedOutUIView& parent) {
    geo.extent.pos += parent.geo.inner.pos;
    geo.inner.pos += geo.extent.pos;
    geo.element.pos += geo.extent.pos;
    geo.content.pos += geo.extent.pos;
}

void UICmdBuffer::push_clip_rect(Rect2D rect) {
    if (clip_rect_stack.length > 0) {
        Rect2D last = clip_rect_stack.last();
        
        rect.pos = glm::max(rect.pos, last.pos);
        rect.size = glm::min(rect.pos + rect.size, last.pos + last.size) - rect.pos;
    }
    clip_rect_stack.append(rect);
}

void UICmdBuffer::pop_clip_rect() {
    clip_rect_stack.pop();
}

glm::vec2 vec2_axis(bool flip, float a, float b) {
    glm::vec2 vec;
    vec[flip] = a;
    vec[!flip] = b;
    return vec;
}

bool handle_events(UI& ui, UI_GUID id, UIElementGeo& geo, Events& events) {
    bool is_hovered = geo.element.intersects(ui.input.mouse_position);
    if (events.on_click && ui.input.mouse_button_pressed() && is_hovered) {
        events.on_click();
        return true;
    }

    static hash_set<UI_GUID, 13> hovered;

    if (events.on_hover) {
        bool was_hovered = hovered.contains(id);
        if (is_hovered && !was_hovered) {
            hovered.add(id);
            events.on_hover(true);
        }

        if (!is_hovered && was_hovered) {
            hovered.remove(id);
            events.on_hover(false);
        }
    }

    return false;
}

void render_bg(UI& ui, UIElementGeo& geo, const BgStyle& bg) {
    UICmdBuffer& cmd_buffer = current_layer(ui);
    if (bg.color.a != 0) draw_quad(cmd_buffer, geo.element.pos, geo.element.size, bg.color);
    
    for(uint i = 0; i < 4; i++) {
        float thickness;
        to_size_bounds(&thickness, bg.border_thickness[i], 100);
        if (thickness == 0.0f) continue;
        
        Rect2D border;
        bool axis = i/2;
        border.pos[!axis] = geo.element.pos[!axis];
        border.pos[axis] = geo.element.pos[axis] + (i%2)*(geo.element.size[axis] - thickness);
        border.size = vec2_axis(axis, thickness, geo.element.size[!axis]);
        
        draw_quad(cmd_buffer, border, bg.border_color);
    }
}

void render_border(UICmdBuffer& cmd_buffer, Rect2D inner, float t) {
    draw_quad(cmd_buffer, inner.pos, glm::vec2(inner.size.x, t), color4(255,100,100));
    draw_quad(cmd_buffer, inner.pos + glm::vec2(0,inner.size.y-t), glm::vec2(inner.size.x, t), color4(255,100,100));
    
    draw_quad(cmd_buffer, inner.pos + glm::vec2(inner.size.x-t,0), glm::vec2(t, inner.size.y), color4(255,100,100));
}

bool render_children(UI& ui, LayedOutUIContainer& container) {
    UICmdBuffer& cmd_buffer = current_layer(ui);
    
    cmd_buffer.push_clip_rect(container.geo.inner);
    bool clicked = false;
    for (LayedOutUIView* child : container.children) {
        clicked |= child->render(ui, container);
    }
    cmd_buffer.pop_clip_rect();
    
    return clicked || handle_events(ui, container.id, container.geo, container.events);
}

bool LayedOutUIContainer::render(UI& ui, LayedOutUIView& parent) {
    to_absolute_position(geo, parent);
   
    render_bg(ui, geo, bg);

    return render_children(ui, *this);
}

bool LayedOutScrollView::render(UI& ui, LayedOutUIView &parent) {
    to_absolute_position(geo, parent);
    
    ScrollState& state = ui.scrolls[id];
    Input& input = ui.input;
    
    float height = geo.content.size.y - geo.extent.size.y;
    
    //if (geo.element.intersects(input.mouse_position)) {
    //    state.offset += input.scroll_offset;
    //    state.offset = glm::clamp(state.offset, 0.0f, height);
    //}
    
    Rect2D scroll_region;
    scroll_region.size = glm::vec2(100,geo.extent.size.y);
    scroll_region.pos = geo.extent.pos + glm::vec2(geo.extent.size.x - scroll_region.size.x,0);
    
    Rect2D scroll_handle;
    scroll_handle.size = glm::vec2(5,100 * geo.inner.size.y / height);
    scroll_handle.pos = geo.extent.pos + glm::vec2(geo.extent.size.x - scroll_handle.size.x, (geo.extent.size.y - scroll_handle.size.y) * state.offset / height);
    
    //printf("Mouse position %f, %f\n", input.mouse_position.x, input.mouse_position.y);
    //printf("Scroll handle %f, %f\n", scroll_handle.pos.x, scroll_handle.pos.y);
    
    //printf("Mouse position %f %f\n", input.mouse_position.x, input.mouse_position.y);
    //printf("Scroll position %f %f\n", scroll_handle.pos.x, scroll_handle.pos.y);
    
    bool draw_scroll_bar = geo.content.size.y > geo.inner.size.y && scroll_region.intersects(input.mouse_position);
    //draw_scroll_bar |= fabs(input.scroll_offset) > 0.01f;
    
    static u64 active_id = 0;
    if (active_id == 0 && scroll_handle.intersects(input.mouse_position) && input.mouse_button_down()) {
        active_id = id;
    }
    
    bool active = active_id == id;
    if (active) {
        draw_scroll_bar = true;
        
        if (!input.mouse_button_down()) {
            active_id = 0;
        } else {
            state.offset += input.mouse_offset.y * height/(geo.extent.size.y - scroll_handle.size.y);
            state.offset = glm::clamp(state.offset, 0.0f, height);
        }
    }
    
    UICmdBuffer& cmd_buffer = current_layer(ui);
    
    render_bg(ui, geo, bg);
    
    active |= render_children(ui, *this);

    //todo make configurable
    if (draw_scroll_bar) draw_quad(cmd_buffer, scroll_handle, glm::vec4(0.5,0.5,0.5,1.0));
    
    return active;
}


bool LayedOutPanel::render(UI& ui, LayedOutUIView& parent) {
    PanelState& state = ui.panels[id];
    Input& input = ui.input;
    
    if (state.first) {
        state.position = geo.element.pos;
        state.size = geo.element.size;
        state.first = false;
    }
    
    to_absolute_position(geo, parent);

    UICmdBuffer& cmd_buffer = current_layer(ui);
    
    render_bg(ui, geo, bg);
    
    Rect2D grab_regions[4];
    
    float grab_thickness = 10;
    
    grab_regions[0].pos = geo.element.pos - glm::vec2(0.5f*grab_thickness,0);
    grab_regions[0].size = glm::vec2(grab_thickness, geo.element.size.y);
    
    grab_regions[1].pos = geo.element.pos + glm::vec2(geo.element.size.x - 0.5f*grab_thickness,0);
    grab_regions[1].size = glm::vec2(grab_thickness, geo.element.size.y);
    
    grab_regions[2].pos = geo.element.pos - glm::vec2(0,0.5f*grab_thickness);
    grab_regions[2].size = glm::vec2(geo.element.size.x, grab_thickness);
    
    grab_regions[3].pos = geo.element.pos + glm::vec2(0, geo.element.size.y - 0.5f*grab_thickness);
    grab_regions[3].size = glm::vec2(geo.element.size.x, grab_thickness);
    
    bool mouse_pressed = input.mouse_button_pressed();

    if (!input.mouse_button_down()) {
        state.active_edge = -1;
    }
    
    for (uint i = 0; i < 4; i++) {
        if (!(flags & (PANEL_RESIZEABLE_BASE << i))) continue;
        bool is_active = state.active_edge == i;
        if (is_active || grab_regions[i].intersects(input.mouse_position)) {
            bool vertical = i / 2 == 1;
            
            if (state.active_edge == -1 || is_active) {
                if (vertical) ui.cursor_shape = CursorShape::VResize;
                else ui.cursor_shape = CursorShape::HResize;
            }
            
            if (state.active_edge == -1 && mouse_pressed) {
                state.active_edge = i;
            }
            
            if (is_active) {
                float max_shrink = gtz(geo.element.size[vertical] - min_size[vertical]);
                float offset = ui.input.mouse_offset[vertical];
            
                if (i % 2 == 0) {
                    offset = glm::min(offset, max_shrink);
                    state.position[vertical] += offset;
                    state.size[vertical] -= offset;
                } else {
                    offset = glm::max(offset, -max_shrink);
                    printf("Shrinking %f, max shrink %f", offset, -max_shrink);
                    state.size[vertical] += offset;
                }
            }
        }
    }
    
    if (state.active_edge == -1 && flags & PANEL_MOVABLE && mouse_pressed && geo.element.intersects(input.mouse_position)) {
        state.active_edge = 4;
    }
    
    if (state.active_edge == 4) {
        state.position += ui.input.mouse_offset;
    }
    
    return render_children(ui, *this);
}

bool LayedOutSplitter::render(UI& ui, LayedOutUIView& parent) {
    to_absolute_position(geo, parent);
    
    Input& input = ui.input;
    SplitterState& state = ui.splitters[id];
    
    Rect2D splitter;
    splitter.pos = geo.element.pos;
    splitter.pos[axis] += state.splitter_offset - 0.5*thickness;
    splitter.size[!axis] = geo.element.size[!axis];
    splitter.size[axis] = thickness;
    
    
    bool is_hovered = splitter.intersects(input.mouse_position);
    if (!state.dragging && is_hovered && input.mouse_button_pressed()) {
        state.dragging = true;
    }
    
    if (!input.mouse_button_down()) state.dragging = false;
    if (state.dragging) state.splitter_offset += input.mouse_offset[axis];
    if (is_hovered || state.dragging) ui.cursor_shape = axis ? CursorShape::VResize : CursorShape::HResize;
    
    UICmdBuffer& cmd_buffer = current_layer(ui);    
    draw_quad(cmd_buffer, splitter, is_hovered || state.dragging ? hover_color : color);
    
    bool clicked = false;
    cmd_buffer.push_clip_rect(geo.inner);
    for (uint i = 0; i < 2; i++) clicked |= children[i]->render(ui, *this);
    cmd_buffer.pop_clip_rect();
    return clicked;
}


bool LayedOutText::render(UI& ui, LayedOutUIView& parent) {
    to_absolute_position(geo, parent);

    UICmdBuffer& cmd_buffer = current_layer(ui);
    
    bool clicked = handle_events(ui, parent.id, geo, events);
    render_bg(ui, geo, bg);

    for (auto& word : text.words) {
        draw_text(cmd_buffer, word.text, geo.content.pos + word.pos, *text.font, text.font_scale, text.font_color);
    }
    
    return false;
}

bool LayedOutImage::render(UI& ui, LayedOutUIView& parent) {
    to_absolute_position(geo, parent);
    
    UICmdBuffer& cmd_buffer = current_layer(ui);

    handle_events(ui, id, geo, events);
    render_bg(ui, geo, bg);
    
    cmd_buffer.push_clip_rect(geo.inner);
    draw_quad(cmd_buffer, geo.content.pos, geo.content.size, texture, image_color, a, b);
    cmd_buffer.pop_clip_rect();
    
    return false;
}

void click_text(UI& ui, LayedOutUIView& view, FontInfo& font, glm::vec2 inner_pos, UI_GUID id, char* buffer) {
    Input& input = ui.input;
    
    ui.cursor_shape = CursorShape::IBeam;
    ui.cursor_pos = -1;
    
    float mouse_pos = input.mouse_position.x - inner_pos.x;

    string_view s = buffer;
    
    float x = 0;
    for (int i = 0; i < s.length; i++) {
        Character ch = font.font->chars[s[i]];
        float advance = (ch.advance >> 6) * font.font_scale.x;
        
        if (mouse_pos < x + 0.5f * advance) {
            ui.cursor_pos = i;
            break;
        }
        
        x += advance;
    }
    
    if (ui.cursor_pos == -1) {
        ui.cursor_pos = s.length;
    }
    
    ui.active = id;
    ui.active_input = &view;
    memcpy(ui.buffer, buffer, 100);
}

bool edit_text(UI& ui, LayedOutUIView& view, glm::vec4 cursor_color, FontInfo& font, char* buffer) {
    ui.active_input = &view;
    
    char copy_buffer[100];
    memcpy(copy_buffer, buffer, 100);
    
    int length = strlen(buffer);
    
    bool modified = false;
    
    for (int i = 0; i < 256; i++) {
        if (ui.input.key_pressed((Key)i)) {
            char c = i;
            if (c >= 'A' && c <= 'Z') {
                c += 32;
            }
    
            buffer[ui.cursor_pos] = c;
            memcpy(buffer + ui.cursor_pos + 1, copy_buffer + ui.cursor_pos, length - ui.cursor_pos);
            length++;
            ui.cursor_pos++;
            modified = true;
        }
    }
    if (ui.input.key_pressed(Key::Backspace)) {
        memcpy(buffer, copy_buffer, fmaxf(0.0f, ui.cursor_pos - 1));
        memcpy(buffer + ui.cursor_pos - 1, copy_buffer + ui.cursor_pos, length - ui.cursor_pos);
        
        modified = true;
        
        if (ui.cursor_pos != 0) {
            ui.cursor_pos--;
            length--;
        }
    }
    if (ui.input.key_pressed(Key::Right)) {
        ui.cursor_pos = max(0, ui.cursor_pos + 1);
    }
    if (ui.input.key_pressed(Key::Left)) {
        ui.cursor_pos = min(length, (int)ui.cursor_pos - 1);
    }
    
    buffer[length] = '\0';
    
    memcpy(ui.buffer, buffer, 100);
    
    Rect2D cursor;
    cursor.size.x = 1; //todo make configurable
    cursor.size.y = view.geo.inner.size.y * 1.5;
    cursor.pos.x = view.geo.inner.pos.x;
    cursor.pos.y = view.geo.element.pos.y + center(view.geo.element.size.y, cursor.size.y);
    
    //cursor.pos.x += 0.0f;
    //cursor.pos.y += font.font->chars['X'].size.y * font.font_scale.y;
    
    for (uint i = 0; i < ui.cursor_pos; i++) {
        cursor.pos.x += (font.font->chars[buffer[i]].advance >> 6) * font.font_scale.x;
    }
    
    draw_quad(current_layer(ui), cursor, cursor_color);
    
    return modified;
}


bool LayedOutInputString::render(UI& ui, LayedOutUIView& parent) {
    to_absolute_position(geo, parent);
    
    char* buffer;
    if (input.type == InputString::CString) {
        buffer = input.cstring_buffer; // (ui->active == id) ? ui->buffer : widget->buffer;
    }
    if (input.type == InputString::StringBuffer) {
        input.string_buffer->reserve(input.max);
        buffer = input.string_buffer->data;
    }
    if (input.type == InputString::SString) {
        buffer = input.sstring->data;
    }

    render_bg(ui, geo, bg);
    
    bool hovered = geo.element.intersects(ui.input.mouse_position);
    if (hovered) ui.cursor_shape = CursorShape::IBeam;
    
    bool pressed = hovered && ui.input.mouse_button_pressed();
    if (pressed) click_text(ui, *this, font, geo.inner.pos, id, buffer);

    if (ui.active == id) { edit_text(ui, *this, input.cursor, font, buffer); }
    
    UICmdBuffer& cmd_buffer = current_layer(ui);
    if (strlen(buffer) == 0) {
        draw_text(cmd_buffer, input.placeholder, geo.inner.pos, *font.font, font.font_scale, color4(150,150,150,1));
    } else {
        draw_text(cmd_buffer, buffer, geo.inner.pos, font);
    }
    
    return pressed;
}

bool draw_quad_clicked(UI& ui, Rect2D rect, glm::vec4 color, glm::vec4 hover_color) {
    bool is_hovered = rect.intersects(ui.input.mouse_position);
    if (is_hovered) color = hover_color;
    
    draw_quad(current_layer(ui), rect, color);
    return is_hovered && ui.input.mouse_button_pressed();
}


bool draw_input_number(UI& ui, LayedOutUIView& view, UI_GUID id, Color hover_color, Color cursor_color, FontInfo& font, char* buffer, bool* incr, bool* decr, float* dragged) {
    Input& input = ui.input;
    UIElementGeo& geo = view.geo;

    //Initialize text buffer
    if (ui.active == id) {
        memcpy(buffer, ui.buffer, 100);
    }
    
    //UI calculation values
    glm::vec4 bg_color = view.bg.color;
    glm::vec2 char_size = calc_size_of(font, "<");
    
    Rect2D arrow_left;
    Rect2D arrow_right;
    Rect2D value;
    
    float padding = geo.element.size.x - geo.inner.size.x; //todo padding values can differ for each size
    
    arrow_left.size.x = char_size.x + padding;
    if (ui.active == id) {  //when active arrows are hidden
        arrow_left.size.x = 0;
    }
    
    arrow_left.size.y = geo.element.size.y;
    arrow_left.pos = geo.element.pos;
    
    arrow_right.size = arrow_left.size;
    arrow_right.pos = geo.element.pos + glm::vec2(geo.element.size.x - arrow_right.size.x,0);
    
    
    value.size.x = geo.element.size.x - arrow_left.size.x - arrow_right.size.x;
    value.size.y = geo.element.size.y;
    value.pos.x = geo.element.pos.x + arrow_left.size.x;
    value.pos.y = geo.element.pos.y;
    
    bool hovered = geo.element.intersects(input.mouse_position);
    bool value_hovered = value.intersects(input.mouse_position);
    
    if (hovered) { //SET CURSORS
        draw_quad_clicked(ui, value, bg_color, hover_color);
        
        if (value_hovered) ui.cursor_shape = CursorShape::HResize;
        else ui.cursor_shape = CursorShape::Hand;
    } else {
        draw_quad_clicked(ui, geo.element, bg_color, hover_color);
    }
    
    
    bool pressed = input.mouse_button_pressed() && value_hovered;
    
    UICmdBuffer& cmd_buffer = current_layer(ui);
    
    if (ui.active == id) {
        draw_quad(cmd_buffer, geo.element, hover_color);
        
        if (pressed) click_text(ui, view, font, geo.inner.pos, id, buffer);
        else edit_text(ui, view, cursor_color, font, buffer);
        
        draw_text(cmd_buffer, buffer, geo.inner.pos, font);
    } else {
        glm::vec2 text_pos;
        glm::vec2 takes_up = calc_size_of(font, buffer);
        text_pos = value.pos + center(value.size, takes_up);
        
        bool value_hovered = value.intersects(input.mouse_position);
        bool text_hovered = Rect2D{text_pos, takes_up}.intersects(input.mouse_position);
        
        if (text_hovered) ui.cursor_shape = CursorShape::IBeam;
        else if (value_hovered) ui.cursor_shape = CursorShape::HResize;
        else if (hovered) ui.cursor_shape = CursorShape::Hand;
        
        
        if (pressed && text_hovered) click_text(ui, view, font, text_pos, id, buffer);
        else if (pressed && ui.dragging == 0) {
            ui.dragging = id;
        }
        
        bool is_dragging = ui.dragging == id;
        if (is_dragging && !ui.input.mouse_button_down()) {
            is_dragging = false;
            ui.dragging = 0;
        }
        
        if (is_dragging) {
            *dragged = input.mouse_offset.x;
        }
        
        draw_text(cmd_buffer, buffer, text_pos, font);
        
        if (hovered && !is_dragging) { //draw arrows
            *decr = draw_quad_clicked(ui, arrow_left, bg_color, hover_color);
            *incr = draw_quad_clicked(ui, arrow_right, bg_color, hover_color);
            
            float arrow_pos_y = arrow_left.pos.y + center(arrow_left.size.y, char_size.y);
            
            
            draw_text(cmd_buffer, "<", glm::vec2(geo.inner.pos.x, arrow_pos_y), font);
            draw_text(cmd_buffer, ">", glm::vec2(geo.inner.pos.x + geo.inner.size.x - char_size.x, arrow_pos_y), font);
        }
    }
    
    return pressed; //todo events
}

bool LayedOutInputInt::render(UI& ui, LayedOutUIView& parent) {
    to_absolute_position(geo, parent);
    
    char buffer[100];
    sprintf(buffer, "%i", *input.value);
    bool incr = false, decr = false;
    float dragged = 0.0f;
    
    bool clicked = draw_input_number(ui, *this, id, input.hover, input.cursor, font, buffer, &incr, &decr, &dragged);
    if (incr) *input.value += input.inc;
    if (decr) *input.value -= input.inc;
    *input.value += dragged / input.px_per_inc;
    
    return clicked;
}

bool LayedOutInputFloat::render(UI& ui, LayedOutUIView& parent) {
    to_absolute_position(geo, parent);
    
    char buffer[100];
    sprintf(buffer, "%.3f", *input.value);
    bool incr = false, decr = false;
    float dragged = 0.0f;

    bool clicked = draw_input_number(ui, *this, id, input.hover, input.cursor, font, buffer, &incr, &decr, &dragged);
    if (incr) *input.value += input.inc;
    if (decr) *input.value -= input.inc;
    *input.value += dragged / input.px_per_inc;
    
    return clicked;
}

void LayedOutInputFloat::loose_focus(UI& ui) {
    *input.value = atof(ui.buffer);
}

void LayedOutInputInt::loose_focus(UI& ui) {
    *input.value = atoi(ui.buffer);
}

void LayedOutInputString::loose_focus(UI& ui) {
    if (input.type == InputString::CString) memcpy(input.cstring_buffer, ui.buffer, input.max);
    if (input.type == InputString::StringBuffer) *input.string_buffer = ui.buffer;
    if (input.type == InputString::SString) *input.sstring = ui.buffer;
}

bool LayedOutGeometry::render(UI& ui, LayedOutUIView& parent) {
    to_absolute_position(geo, parent);

    GeometryState& state = ui.geometries[id];
    
    if (state.position != geo.extent.pos || state.size != geo.extent.size) {
        change(geo.extent.pos * ui.draw_data.px_to_screen, geo.extent.size * ui.draw_data.px_to_screen);
        
        state.position = geo.extent.pos;
        state.size = geo.extent.size;
    }
    
    return children[0]->render(ui, *this);
}