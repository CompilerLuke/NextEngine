#include "engine/engine.h"
#include "engine/application.h"
#include "graphics/rhi/window.h"
#include "graphics/renderer/renderer.h"
#include "ecs/ecs.h"
#include "components/transform.h"
#include "components/flyover.h"
#include "components/camera.h"
#include "ui/ui.h"

#include "visualization/debug_renderer.h"
#include "visualization/render_backend.h"
#include "visualization/visualizer.h"
#include "visualization/input_mesh_viewer.h"
#include "solver.h"
#include "mesh.h"
#include "cfd_components.h"

#include "graphics/rhi/frame_buffer.h"
#include "graphics/rhi/rhi.h"

#include "graphics/assets/assets.h"

#include "ui/draw.h"
#include "editor/lister.h"
#include "editor/inspector.h"
#include "editor/selection.h"
#include "cfd_ids.h"

#include "core/job_system/job.h"

#include "editor/viewport.h"
#include "editor/viewport_interaction.h"

#include "mesh_generation/parametric_shapes.h"

#include "math_ia.h"

#include "solver/testing.h"

struct CFD {
    Modules* modules;
	CFDSolver solver;
    Lister* lister;
    Inspector* inspector;
    CFDRenderBackend* backend;
    CFDDebugRenderer* debug_renderer;
    
    Simulation* simulation = nullptr;
	
    InputMeshRegistry mesh_registry;
    CFDVisualization* visualization;
    InputMeshViewer input_mesh_viewer;

    SceneViewport scene_viewport;

    UI* ui;
    UIRenderer* ui_renderer;

    texture_handle mesh;
    texture_handle play;
    texture_handle pause;
    
    FlowQuantity flow_quantity;
    
    atomic_counter solver_job = 0;
    
    MathIA* math_ia;

    CFD(Modules& modules);
};

CFD::CFD(Modules& modules) 
    : modules(&modules), 
    scene_viewport(*modules.window),
    backend(make_cfd_render_backend({})),
    debug_renderer(make_cfd_debug_renderer(*backend)),
    input_mesh_viewer(*modules.world, *backend, mesh_registry, scene_viewport),
    mesh_registry(*debug_renderer)
{}

void set_theme(UITheme& theme) {
    theme
    .color(ThemeColor::Text, white)
    .color(ThemeColor::Button, shade3)
    .color(ThemeColor::ButtonHover, shade3)
    .color(ThemeColor::Panel, shade1)
    .color(ThemeColor::Splitter, shade1)
    .color(ThemeColor::SplitterHover, blue)
    .color(ThemeColor::Input, shade1)
    .color(ThemeColor::InputHover, shade3)
    .color(ThemeColor::Cursor, blue)
    .size(ThemeSize::DefaultFontSize, 10)
    .size(ThemeSize::Title1FontSize, 13)
    .size(ThemeSize::Title2FontSize, 12)
    .size(ThemeSize::Title3FontSize, 11)
    .size(ThemeSize::SplitterThickness, 1)
    .size(ThemeSize::VStackMargin, 0)
    .size(ThemeSize::HStackMargin, 0)
    .size(ThemeSize::VStackPadding, 4)
    .size(ThemeSize::HStackPadding, 0)
    .size(ThemeSize::StackSpacing, 4)
    .size(ThemeSize::VecSpacing, 2)
    .size(ThemeSize::TextPadding, 0)
    .size(ThemeSize::InputPadding, 4)
    .size(ThemeSize::InputMinWidth, 60)
    .size(ThemeSize::InputMaxWidth, {Perc, 20});
}

void default_scene(Lister& lister, InputMeshRegistry& registry, World& world) {
    Transform model_trans;
    //model_trans.position.x = 4.375;
    model_trans.scale = glm::vec3(1);
    //model_trans.scale = glm::vec3(3);
    model_trans.rotation = glm::angleAxis(to_radians(-90.0f), glm::vec3(1, 0, 0));
    
    input_model_handle model;
    //= registry.load_model("part.fbx", compute_model_matrix(model_trans));

    if (false) {
        auto [e, trans, mesh] = world.make<Transform, CFDMesh>();
        mesh.model = model;
        mesh.color = glm::vec4(1,1,0,1);

        register_entity(lister, "Mesh", e.id);
    }
    {
        auto [e, trans, domain] = world.make<Transform, CFDDomain>();
        domain.size = vec3(100);
        domain.tetrahedron_layers = 3;
        domain.contour_initial_thickness = 0.5;
        domain.contour_thickness_expontent = 1.4;

        domain.feature_angle = 45;
        domain.min_feature_quality = 0.6;
        
        register_entity(lister, "Domain", e.id);
    }
    {
        auto [e, trans, camera, flyover] = world.make<Transform, Camera, Flyover>();
        flyover.mouse_sensitivity = 0.1f;
        //flyover.movement_speed *= 0.1;
        trans.position.z = 15.0;
    }
}

void test_front();
void insphere_test();

APPLICATION_API CFD* init(void* args, Modules& engine) {
    //insphere_test();
    
    if (engine.headless) {
        CFDDebugRenderer* debug = nullptr;
        
        Testing test(*debug);
        test_solver(test);
        return nullptr;
    }
    
    World& world = *engine.world;


    CFDRenderBackendOptions options = {};
    
    CFD* cfd = new CFD(engine);
    cfd->modules = &engine;
    cfd->ui = make_ui();
    cfd->ui_renderer = make_ui_renderer();
    //cfd->backend = make_cfd_render_backend(options);
    cfd->visualization = make_cfd_visualization(*cfd->backend);
    //cfd->debug_renderer = make_cfd_debug_renderer(*cfd->backend);
    cfd->lister = make_lister(world, cfd->scene_viewport.scene_selection, *cfd->ui);
    cfd->inspector = make_inspector(world, cfd->scene_viewport.scene_selection, *cfd->ui, *cfd->lister);
    
    set_theme(get_ui_theme(*cfd->ui));
    
    load_font(*cfd->ui, "editor/fonts/segoeui.ttf");
    cfd->play = load_Texture("play_icon.png");
    cfd->pause = load_Texture("pause_icon.png");
    cfd->mesh = load_Texture("mesh_icon.png");
    
    cfd->math_ia = make_math_ia(cfd->ui, cfd->debug_renderer);
    
    default_scene(*cfd->lister, cfd->mesh_registry, world);
    
    //todo move out of application init and into engine init
    Dependency dependencies[1] = {
        { FRAGMENT_STAGE, RenderPass::Scene },
    };
    
    build_framegraph(*engine.renderer, { dependencies, 1});
    end_gpu_upload();
    
	return cfd;
}

APPLICATION_API bool is_running(CFD& cfd, Modules& modules) {
	return !modules.window->should_close();
}

APPLICATION_API void enter_play_mode(CFD& cfd, Modules& modules) {
	cfd.solver.phase = SOLVER_PHASE_NONE;
}

void generate_mesh_in_background(CFD& cfd) {
    World& world = *cfd.modules->world;

    auto some_mesh = world.first<Transform, CFDMesh>();
    auto some_domain = world.first<Transform, CFDDomain>();
    
    // && some_mesh
    if (!(some_domain)) return;

    //auto [e1, mesh_trans, mesh] = *some_mesh;
    auto [e2, domain_trans, domain] = *some_domain;

    vec4 plane(domain.plane, dot(domain.plane, domain.center));

    cfd.solver.phase = SOLVER_PHASE_MESH_GENERATION;
    cfd.solver.results = {};
    
    CFDMeshError err;
    //cfd.solver.mesh = generate_mesh(*cfd.modules->world, cfd.mesh_registry, err, *cfd.debug_renderer);
    cfd.solver.mesh = generate_parametric_mesh();

    if (err.type == CFDMeshError::None) {
        cfd.solver.phase = SOLVER_PHASE_SIMULATION;
    }
    else {
        log_error(err);
        cfd.solver.phase = SOLVER_PHASE_FAIL;
    }

    build_vertex_representation(*cfd.visualization, cfd.solver.mesh,  plane, cfd.flow_quantity, cfd.solver.results, true);
}

void solve_in_background(CFD& cfd) {
    printf("Wait counter value %i\n", cfd.solver_job.load());
    wait_for_counter(&cfd.solver_job, 1);
    
    cfd.solver.phase = SOLVER_PHASE_SIMULATION;
    
    auto some_domain = cfd.modules->world->first<Transform, CFDDomain>();
    if (!some_domain) return;
    
    auto [e2, domain_trans, domain] = *some_domain;
    vec4 plane(domain.plane, dot(domain.plane, domain.center));

    if (!cfd.simulation) cfd.simulation = make_simulation(cfd.solver.mesh, *cfd.debug_renderer);
    
    real dt = 1.0 / 100;
    uint time_steps = 1000.0 / dt;
    
    for (uint i = 0; i < time_steps; i++) {
        if (cfd.solver.phase == SOLVER_PHASE_PAUSE_SIMULATION) {
            break;
        }
        if (cfd.solver.phase != SOLVER_PHASE_SIMULATION) {
            printf("QUIT SIMULATION!\n");
            break;
        }
        
        cfd.solver.results = simulate_timestep(*cfd.simulation, dt);
        build_vertex_representation(*cfd.visualization, cfd.solver.mesh, plane, cfd.flow_quantity, cfd.solver.results, true);
        //break;
        suspend_execution(*cfd.debug_renderer);
    }
    
    destroy_simulation(cfd.simulation);
    cfd.simulation = nullptr;
}

/*APPLICATION_API void reload(CFD& cfd, Modules& modules) {
    if (cfd.solver.phase == SOLVER_PHASE_SIMULATION) {
        cfd.solver.phase = SOLVER_PHASE_COMPLETE;
        
        JobDesc job(solve_in_background, &cfd);
        add_jobs(PRIORITY_HIGH, job, &cfd.solver_job);
    }
}*/

APPLICATION_API void update(CFD& cfd, Modules& modules) {
	World& world = *modules.world;
	CFDSolver& solver = cfd.solver;

	UpdateCtx update_ctx(*modules.time, *modules.input);
	update_ctx.layermask = EntityQuery().with_none(EDITOR_ONLY);
	update_flyover(world, update_ctx);
    handle_scene_interactions(world, cfd.mesh_registry, cfd.scene_viewport, *cfd.debug_renderer);
   
    auto some_domain = world.first<Transform, CFDDomain>();

    if (solver.phase > SOLVER_PHASE_MESH_GENERATION) {
        auto [e2, domain_trans, domain] = *some_domain;
        vec4 plane(domain.plane, dot(domain.plane, domain.center));
        build_vertex_representation(*cfd.visualization, solver.mesh, plane, cfd.flow_quantity, solver.results, false);
    }
    
}

void nav_bar_li(UI& ui, string_view name) {
    text(ui, name)
    .font(12)
    .color(white);
}

StackView& menu_bar(UI& ui) {
    StackView& stack = begin_hstack(ui, 20)
        .width({ Perc,100 });
    {
        text(ui, "File");
        text(ui, "Edit");
        text(ui, "Settings");
    }
    end_hstack(ui);
    
    return stack;
}

ImageView& icon(UI& ui, texture_handle texture) {
    bool& hovered = get_state<bool>(ui);
    
    return image(ui, texture)
        .resizeable()
        .frame(20, 20)
        .color(hovered ? blue : white)
        .on_hover([&](bool hover) { hovered = hover; })
        .flex(glm::vec2(0));
}

void flow_button(CFD& cfd, string_view name, FlowQuantity quantity) {
    UI& ui = *cfd.ui;
    bool& hovered = get_state<bool>(ui);
    
    auto& b = button(ui, name)
    .on_click([&cfd, name, quantity] {
        printf("Quantity %s\n", name.data);
        cfd.flow_quantity = quantity;
    })
    .on_hover([&](bool hover) {
        hovered = hover;
    });
    
    if (cfd.flow_quantity == quantity) b.background(blue);
}

void render_editor_ui(CFD& cfd, Modules& engine) {
    UI& ui = *cfd.ui;
    Lister& lister = *cfd.lister;
    Inspector& inspector = *cfd.inspector;
    int width, height, fb_width, fb_height, dpi_h, dpi_v;

    engine.window->get_window_size(&width, &height);
    engine.window->get_framebuffer_size(&fb_width, &fb_height);
    engine.window->get_dpi(&dpi_h, &dpi_v);

    ScreenInfo screen_info = {};
    screen_info.window_width = width;
    screen_info.window_height = height;
    screen_info.fb_width = fb_width;
    screen_info.fb_height = fb_height;
    screen_info.dpi_h = dpi_h;
    screen_info.dpi_v = dpi_v;
    
    //render_math_ia(*cfd.math_ia);

    begin_ui_frame(ui, screen_info, *engine.input, CursorShape::Arrow);
    set_theme(get_ui_theme(ui));

    begin_vstack(ui, 0)
        .width({ Perc,100 })
        .height({ Perc,100 })
        .padding(0)
        .background(shade2);

    begin_vstack(ui, HCenter).background(shade1);
        menu_bar(ui);
        begin_zstack(ui);
            begin_hstack(ui, 1);
                spacer(ui);
                icon(ui, cfd.mesh)
                    .background(shade2)
                    .on_click([&] {
                        JobDesc job(generate_mesh_in_background, &cfd);
                        add_jobs(PRIORITY_HIGH, job, nullptr);
                    });
                icon(ui, cfd.play)
                    .background(shade2)
                    .on_click([&] {
                        JobDesc job(solve_in_background, &cfd);
                        add_jobs(PRIORITY_HIGH, job, &cfd.solver_job);
                    });
                icon(ui, cfd.pause).background(shade2);
                spacer(ui);
            end_hstack(ui);
            begin_hstack(ui, 1);
                spacer(ui);
                flow_button(cfd, "Velocity", FlowQuantity::Velocity);
                flow_button(cfd, "Pressure", FlowQuantity::Pressure);
                flow_button(cfd, "Pressure Gradient", FlowQuantity::PressureGradient);
            end_hstack(ui);
        end_zstack(ui);
    end_vstack(ui);

    begin_hsplitter(ui).width({ Perc,100 }).flex(glm::vec2(0, 1));
    {
        begin_hsplitter(ui);
            render_lister(lister);
            
            begin_geo(ui, [&](glm::vec2 pos, glm::vec2 size) { cfd.scene_viewport.update(pos, size); });
            image(ui, engine.renderer->scene_map)
                .resizeable();
            end_geo(ui);
        
        end_hsplitter(ui);

        parametric_ui(ui);
        //render_inspector(inspector);
    }
    end_hsplitter(ui);

    end_vstack(ui);

    begin_panel(ui)
        .movable()
        .resizeable()
        .padding(5);

        text(ui, "Debug Viewer").font(15);

        bool& is_hovered = get_state<bool>(ui);
        int& inc = get_state<int>(ui);

        button(ui, "Next")
            .on_click([&]() { resume_execution(*cfd.debug_renderer, inc); })
            .on_hover([&](bool hover) { is_hovered = hover;  })
            .background(is_hovered ? blue : shade2);

        button(ui, "Clear")
            .on_click([&]() { clear_debug_stack(*cfd.debug_renderer); });

        begin_hstack(ui);
            text(ui, "Increments");
            input(ui, &inc);
        end_hstack(ui);

    end_panel(ui);

    end_ui_frame(ui);

    engine.window->set_cursor(get_ui_cursor_shape(ui));
}

APPLICATION_API void extract_render_data(CFD& cfd, Modules& engine, FrameData& data) {
    render_editor_ui(cfd, engine);
    
    Viewport& viewport = cfd.scene_viewport.viewport;
    
    World& world = *engine.world;

    EntityQuery query;
    update_camera_matrices(world, query, viewport);

    fill_pass_ubo(data.pass_ubo, viewport);
    cfd.input_mesh_viewer.extract_render_data(query);
}



APPLICATION_API void render(CFD& cfd, Modules& engine, GPUSubmission& _gpu_submission, FrameData& data) {
    Renderer& renderer = *engine.renderer;
    CFDSolver& solver = cfd.solver;
    CFDRenderBackend& backend = *cfd.backend;
    CFDVisualization& visualization = *cfd.visualization;
    World& world = *engine.world;
    
    RenderPass screen = begin_render_frame();

    //RENDER SCENE
    {
        RenderPass render_pass = begin_cfd_scene_pass(backend, renderer, data);
        CommandBuffer& cmd_buffer = render_pass.cmd_buffer;
        descriptor_set_handle frame_descriptor = get_frame_descriptor(*cfd.backend);
        
        //cfd.input_mesh_viewer.render(cmd_buffer, frame_descriptor);

        //clear_debug_stack(*cfd.debug_renderer);
        //render_math_ia(*cfd.math_ia);
        render_debug(*cfd.debug_renderer, cmd_buffer);

        if (solver.phase > SOLVER_PHASE_MESH_GENERATION) {
            render_cfd_mesh(visualization, cmd_buffer);
        }

        end_render_pass(render_pass);
    }
    
    submit_draw_data(*cfd.ui_renderer, screen.cmd_buffer, get_ui_draw_data(*cfd.ui));
    
    cfd.scene_viewport.input.clear();
    end_render_frame(screen);
}

APPLICATION_API void deinit(CFD* cfd, Modules& engine) {
    if (!cfd) return;
    destroy_UI(cfd->ui);
	delete cfd;
}
