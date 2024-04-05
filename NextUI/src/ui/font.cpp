#include "ui/font.h"
#include "ui/ui.h"

#include "graphics/rhi/rhi.h"
#include "graphics/assets/assets.h"
#include "ui/internal.h"

u64 hash_func(const FontDesc& desc) {
    return desc.font.id | (desc.size << 8);
}

font_handle load_font(FontCache& cache, string_view path) {
    string_buffer real_path = tasset_path(path);
    
    FT_Face face;
    if (FT_New_Face(cache.ft, real_path.data, 0, &face)) {
        return {INVALID_HANDLE};
    }
    
    return cache.fonts.assign_handle(std::move(face));
}

Font make_font(FontCache& cache, const FontDesc& desc) {
    FT_Library ft = cache.ft;
    FT_Face face = *cache.fonts.get(desc.font);
    
    FT_Set_Char_Size(face, 0, desc.size << 6, desc.dpi_h, desc.dpi_v);

    uint glyph_height = face->size->metrics.height >> 6;
    
    uint font_atlas_width = 16;
    
    Image font_atlas;
    font_atlas.num_channels = 4;
    font_atlas.format = TextureFormat::UNORM;
    
    font_atlas.height = glyph_height * 8;
    font_atlas.width = glyph_height * font_atlas_width;
    
    glm::vec2 atlas_size(font_atlas.width, font_atlas.height);
    
    uint row_width = font_atlas.width * 4;
    uint size_of_font_atlas = font_atlas.height * row_width;
    font_atlas.data = TEMPORARY_ZEROED_ARRAY(char, size_of_font_atlas);
    
    printf("[RASTERIZING FONT] (%u) - %upx, %u\n", desc.font.id, desc.size, glyph_height);
    
    Font font;

    for (uint c = 0; c < 128; c++) {
        int atlas_x = c % font_atlas_width;
        int atlas_y = c / font_atlas_width;
        
        atlas_x *= glyph_height;
        atlas_y *= glyph_height;
        
        // Load character glyph
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            fprintf(stderr, "ERROR::FREETYTPE: Failed to load Glyph\n");
            continue;
        }

        unsigned char* pixels = face->glyph->bitmap.buffer;
        
        auto& bitmap = face->glyph->bitmap;

        
        for (uint y = 0; y < bitmap.rows; y++) {
            for (uint x = 0; x < bitmap.width; x++) {
                uint index = x + y * bitmap.width;
                
                uint x_in_atlas = atlas_x + x;
                uint y_in_atlas = atlas_y + y;
                //printf("x = %i, y = %i, index = %i\n", x_in_atlas, y_in_atlas, index);
                
                uint pixel_pos = 4*x_in_atlas + y_in_atlas * row_width;
                
                ((unsigned char*)font_atlas.data)[pixel_pos + 0] = 255;
                ((unsigned char*)font_atlas.data)[pixel_pos + 1] = 255;
                ((unsigned char*)font_atlas.data)[pixel_pos + 2] = 255;
                ((unsigned char*)font_atlas.data)[pixel_pos + 3] = pixels[index];
            }
        }
        
        Character& ch = font.chars[c];
        ch.size = glm::ivec2(bitmap.width, bitmap.rows);
        ch.bearing = glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top);
        ch.advance = face->glyph->advance.x;
        
        ch.a = glm::vec2(atlas_x, atlas_y) / atlas_size;
        ch.b = ch.a + glm::vec2(ch.size) / atlas_size;
        
        //character positioning is now in screen pixels
        
        assert(ch.size.x <= glyph_height && ch.size.y <= glyph_height);
    }

    if (!cache.uploading_font_this_frame) {
        cache.uploading_font_this_frame = true;
        begin_gpu_upload();
    }
    font.atlas = upload_Texture(font_atlas);

    //FT_Done_Face(face);
    //FT_Done_FreeType(ft);
    
    return font;
}

Font& query_font(FontCache& cache, const FontDesc& desc) {
    Font& result = cache.cached[desc];
    
    if (result.atlas.id != INVALID_HANDLE) return result;
    
    result = make_font(cache, desc);
    return result;
}

font_handle load_font(UI& ui, string_view path) {
    font_handle font = load_font(ui.font_cache, path);
    if (font.id == INVALID_HANDLE) {
        throw string_buffer("Could not load font ") + path;
    }
    if (ui.default_font.id == INVALID_HANDLE) {
        ui.default_font = font;
    }
    return font;
}
