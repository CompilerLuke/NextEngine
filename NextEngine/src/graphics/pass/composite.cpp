#include "graphics/pass/composite.h"
#include "graphics/rhi/shader_access.h"
#include "graphics/rhi/pipeline.h"
#include "graphics/rhi/frame_buffer.h"
#include "graphics/rhi/primitives.h"
#include "graphics/assets/assets.h"
#include "graphics/rhi/rhi.h"

void make_composite_resources(CompositeResources& resources, texture_handle depth_scene_map, texture_handle scene_map, texture_handle volumetric_map, texture_handle cloud_map, uint width, uint height) {

    sampler_handle nearest_sampler = query_Sampler({});
    sampler_handle linear_sampler = query_Sampler({Filter::Linear, Filter::Linear});
    
    for (uint i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        UBOBuffer buffer = alloc_ubo_buffer(sizeof(UBOBuffer), UBO_PERMANENT_MAP);
        resources.ubo[i] = buffer;
        
        DescriptorDesc desc = {};
        add_ubo(desc, FRAGMENT_STAGE, buffer, 0);
        add_combined_sampler(desc, FRAGMENT_STAGE, nearest_sampler, depth_scene_map, 1);
        add_combined_sampler(desc, FRAGMENT_STAGE, nearest_sampler, scene_map, 2);
        add_combined_sampler(desc, FRAGMENT_STAGE, linear_sampler, volumetric_map, 3);
        add_combined_sampler(desc, FRAGMENT_STAGE, linear_sampler, cloud_map, 4);
        
        update_descriptor_set(resources.descriptors[i], desc);
    }
    
    
    FramebufferDesc framebuffer_desc{width, height};
    add_color_attachment(framebuffer_desc, &resources.composite_map);
    
    make_Framebuffer(RenderPass::Composite, framebuffer_desc);
    
    shader_handle shader = load_Shader("shaders/screenspace.vert", "shaders/composite.frag");
    GraphicsPipelineDesc pipeline_desc{shader};
    pipeline_desc.render_pass = RenderPass::Composite;
    
    resources.pipeline = query_Pipeline(pipeline_desc);
}

void fill_composite_ubo(CompositeUBO& ubo, Viewport& viewport) {
    ubo.to_depth = glm::inverse(viewport.proj * viewport.view);
}

void render_composite_pass(CompositeResources& resources) {
    RenderPass render_pass = begin_render_pass(RenderPass::Composite);
    bind_pipeline(render_pass.cmd_buffer, resources.pipeline);
    bind_descriptor(render_pass.cmd_buffer, 2, resources.descriptors[get_frame_index()]);
    draw_quad(render_pass.cmd_buffer);
    end_render_pass(render_pass);
}
