#pragma once

#include "engine/handle.h"
#include <glm/vec2.hpp>

struct font_handle {
    uint id = 0;
};

struct Character {
    glm::vec2 a;
    glm::vec2 b;
    glm::ivec2 size;       // Size of glyph
    glm::ivec2 bearing;    // Offset from baseline to left/top of glyph
    uint advance;
};

struct FontDesc {
    font_handle font;
    uint size;
    uint dpi_h;
    uint dpi_v;
    
    bool operator==(const FontDesc& other) const {
        return font.id == other.font.id && size == other.size && dpi_h == other.dpi_h && dpi_v == other.dpi_v;
    }
};

struct Font {
    Character chars[256];
    texture_handle atlas;
    glm::vec2 atlas_size;
};

Font& query_font(struct FontCache& cache, const FontDesc& desc);
