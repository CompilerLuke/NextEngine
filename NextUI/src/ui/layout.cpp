#include "ui/layout.h"
#include "ui/ui.h"
#include "ui/font.h"
#include "ui/internal.h"
#include "graphics/assets/texture.h"

bool is_absolute(Position pos) {
    return pos.mode == AbsolutePx || pos.mode == AbsolutePerc;
}

bool is_relative(Position pos) {
    return pos.mode == RelativePx || pos.mode == RelativePerc;
}

glm::vec2 to_position(Position pos, glm::vec2 size) {
    if (pos.mode == AbsolutePx || pos.mode == RelativePx) return pos.value;
    if (pos.mode == AbsolutePerc || pos.mode == AbsolutePerc) return size * pos.value / 100.0f;
}

void to_size_bounds(float* result, Size size, float max_size) {
    if (size.mode == Px) *result = size.value;
    else if (size.mode == Perc) *result = size.value * max_size / 100.0f;
    else *result = max_size;
}

void to_size_contents(float* result, Size size, float contents, float padding) {
    if (size.mode == Grow) *result = contents + 2.0f * padding;
}

float to_size(Size size, float bounds, float contents, float padding) {
    float result;
    to_size_bounds(&result, size, bounds);
    to_size_contents(&result, size, contents, padding);
    return result;
}


BoxConstraint compute_inner(UI& ui, LayoutStyle& layout, const BoxConstraint constraint, float padding[4], float margin[4]) {
    
    for (uint i = 0; i < 4; i++) {
        to_size_bounds(padding + i, layout.padding[i], constraint.max[i / 2]);
        to_size_bounds(margin  + i, layout.margin[i],  constraint.max[i / 2]);
    }
    
    glm::vec2 min = glm::vec2(0);
    glm::vec2 max = glm::vec2(0);
    
    to_size_bounds(&min.x, layout.min_width,  constraint.max.x);
    to_size_bounds(&max.x, layout.max_width,  constraint.max.x);
    to_size_bounds(&min.y, layout.min_height, constraint.max.y);
    to_size_bounds(&max.y, layout.max_height, constraint.max.y);
    
    min = glm::max(min, constraint.min);
    max = glm::max(min, glm::min(max, constraint.max));
    
    max.x -= padding[ELeading]  + margin[ELeading] + padding[ETrailing] + margin[ETrailing];
    max.y -= padding[ETop]      + margin[ETop]     + padding[EBottom]   + margin[EBottom];
    
    return {min, max};
}

UIElementGeo compute_geometry(UI& ui, LayoutStyle& layout, glm::vec2 content, const BoxConstraint& inner, const BoxConstraint& constraint, float padding[4], float margin[4]) {
    float padding_width = padding[ELeading] + padding[ETrailing];
    float padding_height = padding[ETop] + padding[EBottom];
    
    float margin_width = margin[ELeading] + margin[ETrailing];
    float margin_height = margin[ETop] + margin[EBottom];
    
    glm::vec2 size = glm::clamp(content, inner.min, inner.max);
    
    glm::vec2 element(size.x + padding_width, size.y + padding_height);
    glm::vec2 outer(element.x + margin_width, element.y + margin_height);
    
    UIElementGeo result;
    
    result.inner.size = size;
    result.element.size = element;
    result.content.size = content;
    
    result.extent.size.x = glm::clamp(outer.x, constraint.min.x, constraint.max.x);
    result.extent.size.y = glm::clamp(outer.y, constraint.min.y, constraint.max.y);
    
    result.extent.pos = glm::vec2(0);
    
    result.element.pos.x = 0.5f*gtz(result.extent.size.x - outer.x) * layout.halignment + margin[ELeading];
    result.element.pos.y = 0.5f*gtz(result.extent.size.y - outer.y) * layout.valignment + margin[ETop];
    
    result.inner.pos.x = result.element.pos.x + padding[ELeading];
    result.inner.pos.y = result.element.pos.y + padding[ETop];

    result.content.pos.x = result.inner.pos.x + 0.5f*gtz(size.x - content.x)*layout.halignment;
    result.content.pos.y = result.inner.pos.y + 0.5f*gtz(size.y - content.y)*layout.valignment;

    return result;
}

Font& query_font(UI& ui, float* font_size_ptr, glm::vec2* font_scale, TextStyle& text_style) {
    float font_size = 0.0f;
    to_size_bounds(&font_size, text_style.font_size, ui.max_font_size);
    *font_scale = 1.0f / ui.draw_data.px_to_screen; //Character dimensions are defined in screen pixels, not points (1/72 of an inch), which all layouting is performed in
    
    if (font_size_ptr) *font_size_ptr = font_size;
    
    FontDesc desc;
    desc.font = text_style.font;
    desc.size = font_size;
    desc.dpi_h = ui.dpi_h;
    desc.dpi_v = ui.dpi_v;
    
    if (desc.font.id == INVALID_HANDLE) desc.font = ui.default_font;
    
    return query_font(ui.font_cache, desc);
}

TextLayout compute_text_layout(UI& ui, string_view text, TextStyle& text_style, glm::vec2* size) {
    glm::vec2 font_scale;
    float font_size;
    Font& font = query_font(ui, &font_size, &font_scale, text_style);

    glm::vec2 pos(0);
    float normal_height = font.chars['X'].size.y * font_scale.y;
    float line_spacing = normal_height + font_size * 0.3;

    uint line_begin = 0;
    uint line_width = 0;
    uint line_count = 1;
    
    tvector<TextLayout::Word> words;
    bool justify = text_style.alignment == TJustify;
    
    float max_width = size->x;
    float space_width = (font.chars[' '].advance>>6)*font_scale.x;
    bool wrapped = false;
    
    float max_height = 0.0f;
    float widest_line = 0.0f;
    
    for (uint i = 0; i < text.length;) {
        bool is_space = text[i] == ' ';
        if (is_space) {
            float width = (font.chars[' '].advance >> 6) * font_scale.y;
            pos.x += width;
            line_width += width;
            i++;
            continue;
        }
        if (text[i] == '\n') {
            i++;
            widest_line = max(widest_line, pos.x);
            pos.x = 0;
            pos.y += line_spacing;
            line_begin = words.length;
            line_width = 0.0f;
            line_count++;
            continue;
        }
        
        
        uint word_begin = i;
        float word_width = 0.0f;

        for (; i < text.length && text[i] != ' ' && text[i] != '\n'; i++) {
            Character& ch = font.chars[text[i]];
            word_width += (ch.advance >> 6) * font_scale.x;
            max_height = glm::max(max_height, normal_height + pos.y + (ch.size.y - ch.bearing.y) * font_scale.y);
        }
        
        
        uint word_len = i - word_begin;
        uint line_end = words.length;

        
        bool wrap = pos.x + word_width > max_width && line_end - line_begin > 0 && line_count < text_style.line_limit;
        if (wrap) {
            pos.x = 0;
            pos.y += line_spacing;
            
            float gap = max_width - line_width + space_width;
        
            
            //Justify text spacing
            if (line_end != line_begin) {
                if (justify) {
                    float offset = gap / (line_end - line_begin - 1);
                    for (uint word = line_begin + 1; word < line_end; word++) {
                        words[word].pos.x += (word - line_begin) * offset;
                    }
                } else {
                    float offset = gap * 0.5 * text_style.alignment;
                    for (uint word = line_begin; word < line_end; word++) {
                        words[word].pos.x += offset;
                    }
                }
            }
            
            line_begin = words.length;
            line_end = line_begin;
            line_width = 0.0f;
            line_count++;
            widest_line = max_width;
            wrapped = true;
        }
        
        line_width += word_width;
        words.append({pos, {text.data + word_begin, word_len}});
        
        pos.x += word_width;
    }
    widest_line = max(widest_line, pos.x);
    
    pos.y += line_spacing;
    
    
    TextLayout result;
    result.words = words;
    result.font = &font;
    result.font_scale = font_scale;
    result.font_color = text_style.color;
    
    if (wrapped) size->x = max_width;
    else size->x = widest_line;
    size->y = pos.y;
    
    return result;
}

LayedOutUIView& Text::compute_layout(UI& ui, const BoxConstraint& constraint) {
    float margin[4];
    float padding[4];
    
    BoxConstraint inner = compute_inner(ui, layout, constraint, padding, margin);
    
    glm::vec2 text_size(inner.max);

    LayedOutText& result = alloc_layed_out_view<LayedOutText>(ui,this);
    result.text = compute_text_layout(ui, text, text_style, &text_size);
    result.geo = compute_geometry(ui, layout, text_size, inner, constraint, padding, margin);
    
    return result;
}

void compute_flex_layout(UI& ui, LayedOutUIContainer& result, LayoutStyle& layout, Size spacing_size, uint axis, uint alignment, const BoxConstraint& constraint, slice<UIView*> children) {
    float spacing = 0.0f;
    to_size_bounds(&spacing, spacing_size, constraint.max[axis]);
    
    float margin[4];
    float padding[4];
    
    BoxConstraint inner = compute_inner(ui, layout, constraint, padding, margin);
    BoxConstraint adjusted{glm::vec2(0), inner.max};
    
    float offset = 0;
    float flex_total = 0.0f;
    
    LayedOutUIView** layout_children = alloc_t<LayedOutUIView*>(*ui.allocator, children.length);
    
    float max = 0.0f;
    
    for (uint i = 0; i < children.length; i++) {
        if (i > 0) offset += spacing;
        
        UIView* child = children[i];
        float flex = child->layout.flex_factor[axis];
        if (flex == 0) {
            LayedOutUIView& layout = child->compute_layout(ui, adjusted);
            layout_children[i] = &layout;
            if (is_relative(child->layout.position)) {
                offset += layout.geo.extent.size[axis];
                max = glm::max(max, layout.geo.extent.size[!axis]);
            }
        }
        
        flex_total += flex;
    }
    
    
    float px_per_flex = (adjusted.max[axis] - offset) / flex_total;
    px_per_flex = px_per_flex < 0 ? 0 : px_per_flex;
    
    offset = 0;
    for(uint i = 0; i < children.length; i++) {
        if (i > 0) offset += spacing;
        
        UIView* child = children[i];
        float flex = child->layout.flex_factor[axis];

        if (flex != 0) {
            float fill = flex * px_per_flex;
            //printf("Fill Height %f\n", fill_height);
            BoxConstraint flex_constraint;
            flex_constraint.min[axis] = fill;
            flex_constraint.max[axis] = fill;
            flex_constraint.max[!axis] = inner.max[!axis];
            
            LayedOutUIView& layout = child->compute_layout(ui, flex_constraint);
            layout_children[i] = &layout;
            max = glm::max(max, layout.geo.extent.size[!axis]);
        }
        
        UIElementGeo& geo = layout_children[i]->geo;
        
        if (is_relative(child->layout.position)) {
            geo.extent.pos[axis] = offset;
            offset += geo.extent.size[axis];
        } else {
            geo.extent.pos = glm::vec2(0);
        }
    }
    
    
    glm::vec2 content;
    content[axis] = offset;
    content[!axis] = max;
    
    UIElementGeo geo = compute_geometry(ui, layout, content, inner, constraint, padding, margin);
    
    //align children
    for (uint i = 0; i < children.length; i++) {
        Position position = children[i]->layout.position;
        UIElementGeo& geo = layout_children[i]->geo;
        float remaining = max - geo.extent.size[!axis];
        if (is_relative(position)) geo.extent.pos[!axis] = 0.5f*alignment*remaining;

        glm::vec2 offset = to_position(position, content);
        layout_children[i]->geo.extent.pos += offset;
    }
    
    result.geo = geo;
    result.children = {layout_children, children.length};
}

LayedOutUIView& ImageView::compute_layout(UI& ui, const BoxConstraint& constraint) {
    float padding[4];
    float margin[4];
    
    BoxConstraint inner = compute_inner(ui, layout, constraint, padding, margin);
    
    TextureDesc& desc = *texture_desc(texture);
    glm::vec2 size_r = glm::abs(b-a);
    glm::vec2 size = size_r * glm::vec2(desc.width, desc.height);
    float image_aspect_ratio = size_r.x*desc.width / (size_r.y*desc.height);
    
    if (aspect_ratio == KEEP_ASPECT) aspect_ratio = image_aspect_ratio;
    
    bool locked_x = layout.min_width == layout.max_width;
    bool locked_y = layout.min_height == layout.max_height;
    
    glm::vec2 content;
    if (image_scale == Fixed) {
        if (aspect_ratio == IGNORE_ASPECT) aspect_ratio = image_aspect_ratio;
        
        content = size;
        content.x *= image_aspect_ratio/aspect_ratio;
    }
    else if (aspect_ratio == IGNORE_ASPECT) {
        content = inner.max;
    }
    else if (image_scale == Fill) {
        bool axis = inner.max.x / aspect_ratio < inner.max.y;
        
        content[axis] = inner.max[axis];
        content[!axis] = inner.max[axis] * (axis ? aspect_ratio : 1.0/aspect_ratio);
    } else if (image_scale == Fit) {
        bool axis = inner.max.x / aspect_ratio > inner.max.y;
        
        content[axis] = inner.max[axis];
        content[!axis] = inner.max[axis] * (axis ? aspect_ratio : 1.0/aspect_ratio);
    } else if (image_scale == Frame && (locked_x || locked_y)) {
        bool axis = layout.max_width.mode == Grow;
        
        content[axis] = inner.max[axis];
        content[!axis] = inner.max[axis] * (axis ? aspect_ratio : 1.0/aspect_ratio);
    } else {
        content = glm::clamp(size, inner.min, inner.max);
    }
    
    LayedOutImage& result = alloc_layed_out_view<LayedOutImage>(ui,this);
    result.texture = texture;
    result.image_color = image_color;
    result.geo = compute_geometry(ui, layout, content, inner, constraint, padding, margin);
    result.a = a;
    result.b = b;
    
    return result;
}

//todo change into pointer
LayedOutUIView& StackView::compute_layout(UI& ui, const BoxConstraint& constraint) {
    LayedOutUIContainer& element = alloc_layed_out_view<LayedOutUIContainer>(ui,this);
    compute_flex_layout(ui, element, layout, spacing, axis, alignment, constraint, children);
    return element;
}

LayedOutUIView& ScrollView::compute_layout(UI& ui, const BoxConstraint& constraint) {
    float padding[4];
    float margin[4];
    BoxConstraint inner = compute_inner(ui, layout, constraint, padding, margin);
    
    float spacing = 0.0f;
    
    ScrollState& state = ui.scrolls[guid];
    
    float offset = 0.0;
    uint axis = 1;
    
    LayedOutUIView** layed_out_children = alloc_t<LayedOutUIView*>(*ui.allocator, children.length);
    
    for (uint i = 0; i < children.length; i++) {
        BoxConstraint child_constraint;
        child_constraint.max = glm::vec2(inner.max.x, 100000);
        
        LayedOutUIView& child = children[i]->compute_layout(ui, child_constraint);
        child.geo.extent.pos[axis] = offset + state.offset;
        offset += child.geo.extent.size[axis] + spacing;
        layed_out_children[i] = &child;
    }
    
    if (state.offset > offset) state.offset = offset;
    
    glm::vec2 content(inner.max.x, offset);
    
    LayedOutScrollView& element = alloc_layed_out_view<LayedOutScrollView>(ui, this);
    element.children = {layed_out_children, children.length};
    element.geo = compute_geometry(ui, layout, content, inner, constraint, padding, margin);
    
    return element;
}

LayedOutUIView& PanelView::compute_layout(UI& ui, const BoxConstraint& constraint) {
    float padding[4];
    float margin[4];
    glm::vec2 min_size = compute_inner(ui, layout, constraint, padding, margin).min;
    
    PanelState& state = ui.panels[guid];
    if (!state.first) {
        if (flags & PANEL_MOVABLE) layout.position = {AbsolutePx, state.position};
        for (uint i = 0; i < 4; i++) {
            if (!(flags & (PANEL_RESIZEABLE_BASE << i))) continue;
            if (i % 2 == 0) width(state.size.x);
            else height(state.size.y);
        }
    }
    
    BoxConstraint inner = compute_inner(ui, layout, constraint, padding, margin);
    
    LayedOutPanel& result = alloc_layed_out_view<LayedOutPanel>(ui, this);
    result.flags = flags;
    result.min_size = min_size;
    compute_flex_layout(ui, result, layout, 0.0f, 1, 0, inner, children);

    return result;
}

LayedOutUIView& SplitterView::compute_layout(UI& ui, const BoxConstraint& constraint) {
    SplitterState& state = ui.splitters[guid];
    
    assert_mesg(children.length == 2, "Splitter expects 2 children");
    
    float padding[4];
    float margin[4];
    BoxConstraint inner = compute_inner(ui, layout, constraint, padding, margin);
    
    BoxConstraint a = inner;
    BoxConstraint b = inner;
    
    a.min = glm::vec2(0);
    b.min = glm::vec2(0);
    
    if (state.first) {
        state.splitter_offset = 0.5f*inner.max[axis];
        state.first = false;
    }
    
    float thickness = 1.0f;
    to_size_bounds(&thickness, splitter_thickness, inner.max[axis]);
    
    a.max[axis] = state.splitter_offset - 0.5*thickness;
    b.max[axis] = inner.max[axis] - state.splitter_offset - 0.5*thickness;
    
    StableID child_ids[2];

    LayedOutSplitter& result = alloc_layed_out_view<LayedOutSplitter>(ui, this);
    result.children[0] = &children[0]->compute_layout(ui, a);
    result.children[1] = &children[1]->compute_layout(ui, b);

    result.children[1]->geo.extent.pos[axis] = state.splitter_offset+0.5f*thickness;
    
    result.axis = axis;
    result.color = splitter_color;
    result.hover_color = splitter_hover_color;
    result.thickness = thickness;
    
    glm::vec2 content;
    content[axis] = result.children[0]->geo.extent.size[axis] + result.children[1]->geo.extent.size[axis] + thickness;
    content[!axis] = inner.max[!axis];
    //fmaxf(result->children[0]->geo.extent.size[!axis], result->children[1]->geo.extent.pos[!axis]);
    
    result.geo = compute_geometry(ui, layout, content, inner, constraint, padding, margin);
    
    return result;
}

LayedOutUIView& Spacer::compute_layout(UI& ui, const BoxConstraint& constraint) {
    //todo extract into function
    float margin[4] = {};
    for (uint i = 0; i < 4; i++) {
        to_size_bounds(margin + i, layout.margin[i], constraint.max[i/2]);
    }
    
    LayedOutUIView& result = alloc_layed_out_view<LayedOutUIView>(ui, this);
    result.geo.extent.size = constraint.min;
    result.geo.element.size = constraint.min;
    result.geo.inner.size = constraint.min;
    
    //result->geo.element.size.x = constraint.max.x - margin[ELeading] - margin[ETrailing];
    //result->geo.element.size.y = constraint.max.y - margin[ETop] - margin[EBottom];
    //result->geo.element.pos.x = margin[ELeading];
    //result->geo.element.pos.y = margin[ETrailing];
    
    return result;
}

LayedOutUIView& GeometryView::compute_layout(UI& ui, const BoxConstraint& constraint) {
    LayedOutGeometry& result = alloc_layed_out_view<LayedOutGeometry>(ui, this);
    
    //todo support multiple children
    LayedOutUIView** result_children = TEMPORARY_ARRAY(LayedOutUIView*, 1);
    result_children[0] = &children[0]->compute_layout(ui, constraint);
    
    result.change = std::move(change);
    result.children = { result_children,1 };
    result.geo = result.children[0]->geo;

    return result;
}

glm::vec2 calc_size_of(FontInfo& info, string_view text) {
    Font& font = *info.font;
    glm::vec2 font_scale = info.font_scale;
    float normal_height = font.chars['X'].size.y * font_scale.y;
    
    float offset = 0.0f;
    for (uint i = 0; i < text.length; i++) {
        offset += (font.chars[text[i]].advance >> 6) * font_scale.x;
    }
    return glm::vec2(offset, normal_height);
}

template<typename R, typename T>
R& compute_input_layout(UI& ui, T* self, const BoxConstraint& constraint) {
    float padding[4], margin[4];
    BoxConstraint inner = compute_inner(ui, self->layout, constraint, padding, margin);
    
    R& result = alloc_layed_out_view<R>(ui, self);
    result.input = self->input;
    result.font.font = &query_font(ui, nullptr, &result.font.font_scale, self->text_style);
    result.font.font_color = self->text_style.color;
    
    glm::vec2 content = calc_size_of(result.font, "X");
    content.x = inner.max.x; //todo don't hardcode
    
    result.geo = compute_geometry(ui, self->layout, content, inner, constraint, padding, margin);
    
    return result;
}

LayedOutUIView& InputStringView::compute_layout(UI& ui, const BoxConstraint& constraint) {
    return compute_input_layout<LayedOutInputString, InputStringView>(ui, this, constraint);
}

LayedOutUIView& InputIntView::compute_layout(UI &ui, const BoxConstraint& constraint) {
    return compute_input_layout<LayedOutInputInt, InputIntView>(ui, this, constraint);
}

LayedOutUIView& InputFloatView::compute_layout(UI& ui, const BoxConstraint& constraint) {
    return compute_input_layout<LayedOutInputFloat, InputFloatView>(ui, this, constraint);
}
