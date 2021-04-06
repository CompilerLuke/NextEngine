#include "draw.h"
#include <stdio.h>
#include <stdint.h>
#include <glm/gtc/matrix_transform.hpp>

#include "graphics/rhi/rhi.h"
#include "engine/vfs.h"
#include "core/container/hash_map.h"
#include "graphics/assets/assets.h"

const uint MAX_UI_TEXTURES = 16;
const u64 MAX_UI_VERTEX_BUFFER_SIZE = mb(1);
const u64 MAX_UI_INDEX_BUFFER_SIZE = mb(1);

struct UIRenderer {
    texture_handle font_texture;
    CPUVisibleBuffer vertex_buffer;
    CPUVisibleBuffer index_buffer;
    Arena vertex_arena[MAX_FRAMES_IN_FLIGHT];
    Arena index_arena[MAX_FRAMES_IN_FLIGHT];
    UBOBuffer proj_ubo_buffer;
    descriptor_set_handle descriptor[MAX_FRAMES_IN_FLIGHT];
    sampler_handle sampler;
    texture_handle dummy_tex;
    pipeline_handle pipeline;
};

UIRenderer* make_ui_renderer() {
    //LOAD SHADERS
    
    UIRenderer* renderer = PERMANENT_ALLOC(UIRenderer);
    
    shader_handle shader = load_Shader("shaders/imgui_shader.vert", "shaders/imgui_shader.frag");

    renderer->sampler = query_Sampler({Filter::Linear, Filter::Linear});

    //CREATE VERTEX BUFFER
    VertexLayoutDesc vertex_layout_desc{ {
        { 2, VertexAttrib::Float, offsetof(UIVertex, pos) },
        { 2, VertexAttrib::Float, offsetof(UIVertex, uv) },
        { 4, VertexAttrib::Float, offsetof(UIVertex, col) },
    }, sizeof(UIVertex)
    };

    VertexLayout vertex_layout = register_vertex_layout(vertex_layout_desc);
    
    renderer->dummy_tex = default_textures.white;
    
    alloc_Arena(renderer->vertex_arena, MAX_UI_VERTEX_BUFFER_SIZE, &renderer->vertex_buffer, BUFFER_VERTEX);
    alloc_Arena(renderer->index_arena, MAX_UI_INDEX_BUFFER_SIZE, &renderer->index_buffer, BUFFER_INDEX);

    renderer->proj_ubo_buffer = alloc_ubo_buffer(sizeof(glm::mat4), UBO_PERMANENT_MAP);

    //CREATE DESCRIPTORS
    CombinedSampler sampler_array[MAX_UI_TEXTURES] = {};
    for (uint i = 0; i < MAX_UI_TEXTURES; i++) {
        sampler_array[i].sampler = renderer->sampler;
        sampler_array[i].texture = renderer->dummy_tex;
    }

    DescriptorDesc descriptor_desc;
    add_ubo(descriptor_desc, VERTEX_STAGE, renderer->proj_ubo_buffer, 0);
    add_combined_sampler(descriptor_desc, FRAGMENT_STAGE, { sampler_array, MAX_UI_TEXTURES }, 1);

    for (uint i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        //todo this generates 3 layouts when 1 would suffice
        update_descriptor_set(renderer->descriptor[i], descriptor_desc);
    }
    
    GraphicsPipelineDesc pipeline_desc = {};
    pipeline_desc.render_pass = RenderPass::Screen;
    pipeline_desc.vertex_layout = vertex_layout;
    pipeline_desc.instance_layout = INSTANCE_LAYOUT_NONE;
    pipeline_desc.shader = shader;
    pipeline_desc.state = Cull_None | BlendMode_One_Minus_Src_Alpha;
    pipeline_desc.range[1].size = sizeof(int);

    renderer->pipeline = query_Pipeline(pipeline_desc);
    return renderer;
}

void destroy_ui_renderer(UIRenderer* renderer) {
    dealloc_cpu_visible_buffer(renderer->vertex_buffer);
    dealloc_cpu_visible_buffer(renderer->index_buffer);
}


void submit_draw_data(UIRenderer& renderer, CommandBuffer& cmd_buffer, UIDrawData& data) {
    bind_pipeline(cmd_buffer, renderer.pipeline);
    
    glm::mat4 ortho_proj = glm::ortho(0.0f, (float)data.width_px, 0.0f, (float)data.height_px, -1.0f, 1.0f);

    memcpy_ubo_buffer(renderer.proj_ubo_buffer, &ortho_proj);

    UIVertex* vertex_buffer = (UIVertex*)renderer.vertex_buffer.mapped;
    uint* index_buffer = (uint*)renderer.index_buffer.mapped;
    
    uint frame_index = get_frame_index();

    u64 vertex_offset = renderer.vertex_arena[frame_index].base_offset;
    u64 index_offset = renderer.index_arena[frame_index].base_offset;
    u64 vertices_rendered = 0;
    
    bind_vertex_buffer(cmd_buffer, renderer.vertex_buffer.buffer, vertex_offset);
    bind_index_buffer(cmd_buffer, renderer.index_buffer.buffer, index_offset);

    hash_map<uint, CombinedSampler, MAX_UI_TEXTURES> combined_samplers = {};
    
    sampler_handle nearest = query_Sampler({});

    for (uint i = 0; i < MAX_UI_TEXTURES; i++) {
        combined_samplers.values[i].sampler = nearest;
        combined_samplers.values[i].texture = renderer.dummy_tex;
    }

    // Upload textures
    for (const UICmdBuffer& cmd_list : data.layers) {
        for (const UICmd& cmd : cmd_list.cmds) {
            combined_samplers[cmd.texture.id].texture = cmd.texture;
            //todo eleminate magic value
            if (cmd.texture.id > 100) combined_samplers[cmd.texture.id].sampler = renderer.sampler;
        }
    }

    {
        DescriptorDesc descriptor_desc;
        add_combined_sampler(descriptor_desc, FRAGMENT_STAGE, { combined_samplers.values, MAX_UI_TEXTURES }, 1);
        update_descriptor_set(renderer.descriptor[frame_index], descriptor_desc);

        bind_descriptor(cmd_buffer, 0, renderer.descriptor[frame_index]);
    }

    size_t idx_buffer_offset = 0;
    
    float fb_width = data.width_px * data.px_to_screen.x;
    float fb_height = data.height_px * data.px_to_screen.y;

    // Render command lists
    for (const UICmdBuffer& cmd_list : data.layers) {
        u64 vertex_size = cmd_list.vertices.length * sizeof(UIVertex);
        u64 index_size = cmd_list.indices.length * sizeof(UIIndex);
        
        memcpy((char*)vertex_buffer + vertex_offset, cmd_list.vertices.data, vertex_size);
        memcpy((char*)index_buffer + index_offset, cmd_list.indices.data, index_size);

        for (const UICmd& cmd : cmd_list.cmds) {
            Rect2D clip_rect = cmd.clip_rect;
            clip_rect.pos *= data.px_to_screen;
            clip_rect.size *= data.px_to_screen;
            
            if (clip_rect.pos.x < fb_width && clip_rect.pos.y < fb_height && clip_rect.size.x >= 0.0f && clip_rect.size.y >= 0.0f) {
                int tex_id = combined_samplers.index(cmd.texture.id);
                
                push_constant(cmd_buffer, FRAGMENT_STAGE, 0, sizeof(int), &tex_id);
                set_scissor(cmd_buffer, clip_rect);
                
                VertexBuffer vertex_buffer = {};
                vertex_buffer.length = cmd.count;
                vertex_buffer.index_base = idx_buffer_offset;
                vertex_buffer.vertex_base = vertices_rendered;
                
                draw_mesh(cmd_buffer, vertex_buffer);
            }
            idx_buffer_offset += cmd.count;
        }

        vertex_offset += vertex_size;
        index_offset += index_size;
        vertices_rendered += cmd_list.vertices.length;
    }
}
