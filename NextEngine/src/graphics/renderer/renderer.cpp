#include "engine/engine.h"
#include "graphics/renderer/renderer.h"
#include "graphics/assets/assets.h"
#include "components/camera.h"
#include "graphics/rhi/draw.h"
#include "core/memory/linear_allocator.h"
#include "graphics/pass/pass.h"
#include "graphics/rhi/window.h"
#include "graphics/rhi/frame_buffer.h"
#include "graphics/culling/culling.h"
#include "core/profiler.h"
#include "core/time.h"

//HACK ACKWARD INITIALIZATION
#include "ecs/ecs.h"

//todo move into rhi.h
#include "graphics/rhi/vulkan/vulkan.h"

uint get_frame_index() {
    return rhi.frame_index;
}

RenderSystem::~RenderSystem() {}

RenderPrepareCtx::RenderPrepareCtx(World &world, Viewport viewport, EntityQuery layermask, EntityQuery camera,
                                   RenderPass::ID pass, uint32_t frame)
        : world(world), viewport(viewport), layermask(layermask), frame(frame), pass(pass) {}

uint32_t RenderPrepareCtx::get_frame() { return frame; }

Viewport &RenderPrepareCtx::get_viewport() { return viewport; }

World &RenderPrepareCtx::get_world() { return world; }

EntityQuery RenderPrepareCtx::get_layermask() { return layermask; }

EntityQuery RenderPrepareCtx::get_camera_layermask() { return camera; }

RenderSubmitCtx::RenderSubmitCtx(uint32_t frame, RenderPass screen, RenderPass::ID pass) : frame(frame) {
    pass_stack.append(pass);
    active_passes[RenderPass::Screen] = screen;
}

uint32_t RenderSubmitCtx::get_frame() { return frame; }

void RenderSubmitCtx::push_render_pass(RenderPass::ID pass) {
    return pass_stack.append(pass);
}
void RenderSubmitCtx::pop_render_pass() {
    pass_stack.pop();
}

RenderPass RenderSubmitCtx::get_current_pass() { return get_render_pass(pass_stack.last()); }

RenderPass RenderSubmitCtx::get_render_pass(RenderPass::ID id) {
    if (!active_passes.contains(id)) {
        RenderPass pass = begin_render_pass(id);
        active_passes[id] = pass;
        return pass;
    }
    return active_passes[id];
}

tvector<RenderPass> RenderSubmitCtx::get_active_passes() {
    tvector<RenderPass> result;
    for (auto [key, pass]: active_passes) result.append(pass);
    return result;
}

RenderPass::ID Renderer::get_output_pass_id() {
    return RenderPass::Composite;
}

RenderPass::ID Renderer::get_depth_pass_id() {
    return RenderPass::Composite;
}

texture_handle Renderer::get_output_map() {
    return composite_resources.composite_map;
}

texture_handle Renderer::get_depth_map() {
    return depth_scene_map;
}

void Renderer::add(std::unique_ptr<RenderSystem> &&system) {
    systems.append(std::move(system));
}

void make_scene_pass(Renderer &renderer, uint width, uint height, uint msaa) {
    for (uint i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        renderer.scene_pass_ubo[i] = alloc_ubo_buffer(sizeof(PassUBO), UBO_PERMANENT_MAP);
        renderer.simulation_ubo[i] = alloc_ubo_buffer(sizeof(SimulationUBO), UBO_PERMANENT_MAP);

        DescriptorDesc descriptor_desc = {};
        add_ubo(descriptor_desc, VERTEX_STAGE, renderer.scene_pass_ubo[i], 0);
        add_ubo(descriptor_desc, VERTEX_STAGE | FRAGMENT_STAGE, renderer.simulation_ubo[i], 1);
        update_descriptor_set(renderer.scene_pass_descriptor[i], descriptor_desc);
    }

    FramebufferDesc desc{width, height};
    // todo: enabling stencil buffer, causes strange layout transiton when attempting to copy depth image into
    //  buffer
    //desc.stencil_buffer = StencilBufferFormat::P8;

    AttachmentDesc &attachment = add_color_attachment(desc, &renderer.scene_map);
    attachment.num_samples = msaa; //todo implement msaa for depth settings.msaa;
    attachment.usage = attachment.usage | TextureUsage::TransferSrc;

    AttachmentDesc &depth_attachment = add_depth_attachment(desc, &renderer.depth_scene_map);
    depth_attachment.num_samples = msaa;
    depth_attachment.usage = depth_attachment.usage | TextureUsage::TransferSrc;

    add_dependency(desc, VERTEX_STAGE, RenderPass::TerrainHeightGeneration, TextureAspect::Color);
    add_dependency(desc, FRAGMENT_STAGE, RenderPass::TerrainTextureGeneration, TextureAspect::Color);

    for (uint i = 0; i < 4; i++) {
        add_dependency(desc, FRAGMENT_STAGE, (RenderPass::ID) (RenderPass::Shadow0 + i), TextureAspect::Depth);
    }

    if (renderer.settings.zprepass) {
        SubpassDesc subpasses[2] = {};
        SubpassDesc &z_prepass = subpasses[0];
        z_prepass.depth_attachment = true;

        SubpassDesc &color_pass = subpasses[1];
        color_pass.color_attachments.append(0);
        color_pass.depth_attachment = true;

        make_Framebuffer(RenderPass::Scene, desc, {subpasses, 2});
    } else {
        make_Framebuffer(RenderPass::Scene, desc);
    }

    renderer.z_prepass_pipeline_layout = query_Layout(renderer.scene_pass_descriptor[0]);
}


Renderer::Renderer(const RenderSettings &settings, World &world) : settings(settings) {
    uint width = settings.display_resolution_width;
    uint height = settings.display_resolution_height;

    make_scene_pass(*this, width, height, settings.msaa);

    //return renderer;

    ID skybox = make_default_Skybox(world, "engine/Tropical_Beach_3k.hdr");
    SkyLight *skylight = world.m_by_id<SkyLight>(skybox);

    extract_lighting_from_cubemap(lighting_system, *skylight);

    make_shadow_resources(shadow_resources, simulation_ubo, settings.shadow);
    make_lighting_system(lighting_system, shadow_resources, *skylight);

    array<2, descriptor_set_handle> descriptors = {scene_pass_descriptor[0], lighting_system.pbr_descriptor[0]};
    color_pipeline_layout = query_Layout(descriptors);

    make_volumetric_resources(volumetric_resources, depth_scene_map, shadow_resources, 0.5 * width, 0.5 * height);
    make_composite_resources(composite_resources, depth_scene_map, scene_map, volumetric_resources.volumetric_map,
                             volumetric_resources.cloud_map, width, height);
    init_terrain_render_resources(terrain_render_resources, settings);
}

void build_framegraph(Renderer &renderer, slice<Dependency> dependencies) {
    make_wsi_pass(dependencies);
    build_framegraph();
}

struct GlobalUBO {
    glm::mat4 projection;
    glm::mat4 view;
    float window_width;
    float window_height;
};

void render_overlay(Renderer &renderer, RenderPass &render_pass) {
    render_debug_bvh(renderer.scene_partition, render_pass);
}

RenderPrepareCtx Renderer::extract_render_data(World &world, Viewport &viewport, EntityQuery layermask, EntityQuery
camera_layermask) {
    update_camera_matrices(world, camera_layermask, viewport);
    extract_planes(viewport);

    RenderPrepareCtx prepare(world,
                             viewport,
                             layermask,
                             camera_layermask,
                             RenderPass::Scene,
                             get_frame_index());

    FrameData &frame = frames[prepare.get_frame()];
    frame = {};

    Viewport viewports[RenderPass::ScenePassCount];
    viewports[0] = viewport;

    static bool updated_acceleration = false;
    if (!updated_acceleration) {
        build_acceleration_structure(scene_partition, mesh_buckets, world, settings);
        updated_acceleration = true;
    }


    for (uint i = 0; i < RenderPass::ScenePassCount; i++) {
        frame.culled_mesh_bucket[i] = {TEMPORARY_ZEROED_ARRAY(CulledMeshBucket, MAX_MESH_BUCKETS), MAX_MESH_BUCKETS};
    }

    fill_pass_ubo(frame.pass_ubo, viewport);
    fill_light_ubo(frame.light_ubo, world, viewport, layermask);
    extract_shadow_cascades(frame.shadow_proj_info, viewports + 1, settings.shadow, world, viewport, camera_layermask);
    fill_volumetric_ubo(frame.volumetric_ubo, frame.composite_ubo, world, settings.volumetric, viewport,
                        camera_layermask);
    fill_composite_ubo(frame.composite_ubo, viewport);

    cull_meshes(scene_partition, world, mesh_buckets, RenderPass::ScenePassCount, {frame.culled_mesh_bucket,
                                                                                   MAX_MESH_BUCKETS},
                viewports, layermask, settings);

    extract_grass_render_data(frame.grass_data, world, viewports);
    extract_render_data_terrain(frame.terrain_data, world, &viewport, layermask);
    extract_skybox(frame.skybox_data, world, layermask);

    for (auto &system: systems) {
        system->extract_render_data(prepare);
    }

    return prepare;
}

void bind_scene_pass_z_prepass(Renderer &renderer, RenderPass &render_pass, const FrameData &frame) {
    uint frame_index = get_frame_index();

    SimulationUBO simulation_ubo = {};
    simulation_ubo.time = Time::now();
    memcpy_ubo_buffer(renderer.scene_pass_ubo[frame_index], &frame.pass_ubo);
    memcpy_ubo_buffer(renderer.simulation_ubo[frame_index], &simulation_ubo);

    descriptor_set_handle scene_pass_descriptor = renderer.scene_pass_descriptor[frame_index];

    CommandBuffer &cmd_buffer = *render_pass.cmd_buffer;

    bind_pipeline_layout(cmd_buffer, renderer.z_prepass_pipeline_layout);
    bind_descriptor(cmd_buffer, 0, scene_pass_descriptor);
}

RenderSubmitCtx Renderer::build_command_buffers(RenderPrepareCtx &prepare) {
    Profile profile("Begin Render Frame");
    RenderPass screen = begin_render_frame();
    profile.end();

    load_assets_in_queue();

    for (UpdateMaterial &material: update_materials) {
        update_Material(material.handle, material.from, material.to);
    }
    update_materials.clear();

    if (settings.hotreload_shaders) {
        reload_modified_shaders();
    }

    RenderSubmitCtx submit(prepare.get_frame(), screen, RenderPass::Scene);
    FrameData &frame = frames[submit.get_frame()];

    RenderPass render_passes[RenderPass::ScenePassCount];
    for (int i = 0; i < RenderPass::ScenePassCount; i++) {
        render_passes[i] = submit.get_render_pass(RenderPass::ID(i));
    }

    RenderPass &main_pass = render_passes[RenderPass::Scene];
    CommandBuffer &cmd_buffer = *main_pass.cmd_buffer;

    uint frame_index = get_frame_index();

    //todo move into Frame struct
    ShadowUBO shadow_ubo = {};
    fill_shadow_ubo(shadow_ubo, frame.shadow_proj_info);

    //todo would be more efficient to build structs in place, instead of copying
    memcpy_ubo_buffer(lighting_system.light_ubo[frame_index], &frame.light_ubo);
    memcpy_ubo_buffer(shadow_resources.shadow_ubos[frame_index], &shadow_ubo);
    memcpy_ubo_buffer(composite_resources.ubo[frame_index], &frame.composite_ubo);

    //SHADOW PASS
    bind_cascade_viewports(shadow_resources, render_passes + RenderPass::Shadow0, settings.shadow,
                           frame.shadow_proj_info);

    for (uint i = 0; i < MAX_SHADOW_CASCADES; i++) {
        RenderPass &render_pass = render_passes[RenderPass::Shadow0 + i];
        render_meshes(mesh_buckets, frame.culled_mesh_bucket[RenderPass::Shadow0 + i], render_pass);
        render_grass(frame.grass_data, render_pass);
    }

    //Z-PREPASS
    bind_scene_pass_z_prepass(*this, main_pass, frame);

    if (settings.zprepass) {
        render_meshes(mesh_buckets, frame.culled_mesh_bucket[RenderPass::Scene], main_pass);
        render_grass(frame.grass_data, main_pass);

        next_subpass(main_pass);
    }

    //MAIN PASS
    bind_pipeline_layout(cmd_buffer, color_pipeline_layout);
    bind_descriptor(cmd_buffer, 1, lighting_system.pbr_descriptor[frame_index]);

    for (auto &system: systems) {
        system->build_command_buffers(submit);
    }

    bind_pipeline_layout(cmd_buffer, color_pipeline_layout);
    bind_descriptor(cmd_buffer, 1, lighting_system.pbr_descriptor[frame_index]);

    render_terrain(terrain_render_resources, frame.terrain_data, render_passes);

    //todo partition into lit, unlit, transparent passes

    render_meshes(mesh_buckets, frame.culled_mesh_bucket[RenderPass::Scene], main_pass);
    render_grass(frame.grass_data, main_pass);
    render_skybox(frame.skybox_data, main_pass);

    //Post-Processing
    render_volumetric_pass(volumetric_resources, frame.volumetric_ubo);
    render_composite_pass(composite_resources, submit);

    return submit;
}

void Renderer::end_passes(RenderSubmitCtx &submit) {
    for (RenderPass pass: submit.get_active_passes()) {
        if (pass.id == RenderPass::Screen) continue;
        end_render_pass(pass);
    }
}

void Renderer::submit_frame(RenderSubmitCtx &submit) {
    RenderPass screen = submit.get_render_pass(RenderPass::Screen);
    end_render_frame(screen);
}

Renderer::~Renderer() {}