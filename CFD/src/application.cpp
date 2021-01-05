#include "engine/engine.h"
#include "engine/application.h"
#include "graphics/rhi/window.h"
#include "graphics/renderer/renderer.h"
#include "ecs/ecs.h"
#include "components/transform.h"
#include "components/flyover.h"
#include "components/camera.h"
#include "UI/ui.h"

#include "visualizer.h"
#include "solver.h"
#include "mesh.h"
#include "components.h"

#include "graphics/rhi/frame_buffer.h"
#include "graphics/rhi/rhi.h"

#include "graphics/assets/assets.h"

#include "UI/draw.h"
#include "editor/lister.h"
#include "editor/inspector.h"
#include "editor/selection.h"
#include "cfd_ids.h"

struct CFD {
	CFDSolver solver;
    Lister* lister;
    Inspector* inspector;
    Selection selection;
	CFDVisualization* visualization;
    UI* ui;
    UIRenderer* ui_renderer;

    CFDSceneRenderData render_data; 

    texture_handle mesh;
    texture_handle play;
    texture_handle pause;
};

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

void default_scene(Lister& lister, World& world) {
    model_handle model = load_Model("airfoil.fbx");

    {
        auto [e, trans, mesh] = world.make<Transform, CFDMesh>();
        trans.scale = glm::vec3(1.0);
        trans.rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0, 0.0, 0.0));
        mesh.model = model;
        mesh.color = glm::vec4(1,1,0,1);

        register_entity(lister, "Mesh", e.id);
    }
    {
        auto [e, trans, domain] = world.make<Transform, CFDDomain>();
        register_entity(lister, "Domain", e.id);
    }
    {
        auto [e, trans, camera, flyover] = world.make<Transform, Camera, Flyover>();
        flyover.mouse_sensitivity = 0.1f;
        trans.position.z = 15.0;
    }
}

void test_front();

APPLICATION_API CFD* init(void* args, Modules& engine) {
    World& world = *engine.world;
    
    test_front();

    CFD* cfd = new CFD();
    cfd->ui = make_ui();
    cfd->ui_renderer = make_ui_renderer();
    cfd->visualization = make_cfd_visualization();
    cfd->lister = make_lister(world, cfd->selection, *cfd->ui);
    cfd->inspector = make_inspector(world, cfd->selection, *cfd->ui, *cfd->lister);
    
    set_theme(get_ui_theme(*cfd->ui));
    
    load_font(*cfd->ui, "editor/fonts/segoeui.ttf");
    cfd->play = load_Texture("play_icon.png");
    cfd->pause = load_Texture("pause_icon.png");
    cfd->mesh = load_Texture("mesh_icon.png");
    
    default_scene(*cfd->lister, world);
    
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

APPLICATION_API void update(CFD& cfd, Modules& modules) {
	World& world = *modules.world;
	CFDSolver& solver = cfd.solver;

	UpdateCtx update_ctx(*modules.time, *modules.input);
	update_ctx.layermask = EntityQuery().with_none(EDITOR_ONLY);
	update_flyover(world, update_ctx);

	if (solver.phase == SOLVER_PHASE_MESH_GENERATION) {
		auto some_mesh = world.first<Transform, CFDMesh>();
		auto some_domain = world.first<Transform, CFDDomain>();

		if (some_domain && some_mesh) {
			auto [e1, mesh_trans, mesh] = *some_mesh;
			auto [e2, domain_trans, domain] = *some_domain;
		
			CFDMeshError err;
			solver.mesh = generate_mesh(world, err);
			
			if (err.type == CFDMeshError::None) {
				solver.phase = SOLVER_PHASE_SIMULATION;
			}
			else {
				log_error(err);
				solver.phase = SOLVER_PHASE_FAIL;
			}

			build_vertex_representation(*cfd.visualization, solver.mesh);
		}
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

    begin_ui_frame(ui, screen_info, *engine.input, CursorShape::Arrow);
    set_theme(get_ui_theme(ui));

    begin_vstack(ui, 0)
        .width({ Perc,100 })
        .height({ Perc,100 })
        .padding(0)
        .background(shade2);

    begin_vstack(ui, HCenter).background(shade1);
        menu_bar(ui);
        begin_hstack(ui, 1);
            icon(ui, cfd.mesh)
                .background(shade2)
                .on_click([&] { cfd.solver.phase = SOLVER_PHASE_MESH_GENERATION; });
            icon(ui, cfd.play).background(shade2);
            icon(ui, cfd.pause).background(shade2);
        end_hstack(ui);
    end_vstack(ui);

    begin_hsplitter(ui).width({ Perc,100 }).flex(glm::vec2(0, 1));
    {
        begin_hsplitter(ui);
            render_lister(lister);
            image(ui, engine.renderer->scene_map)
                .scale_to_fill();
        end_hsplitter(ui);

        render_inspector(inspector);
    }
    end_hsplitter(ui);


    end_vstack(ui);

    end_ui_frame(ui);

    engine.window->set_cursor(get_ui_cursor_shape(ui));
}

APPLICATION_API void extract_render_data(CFD& cfd, Modules& engine, FrameData& data) {
    Viewport viewport = {};
    viewport.width = engine.window->width;
    viewport.height = engine.window->height;
    
    World& world = *engine.world;

    cfd.render_data = {};

    EntityQuery query;
    update_camera_matrices(world, query, viewport);

    fill_pass_ubo(data.pass_ubo, viewport);
    extract_cfd_scene_render_data(cfd.render_data, world, query);
    render_editor_ui(cfd, engine);
}



APPLICATION_API void render(CFD& cfd, Modules& engine, GPUSubmission& _gpu_submission, FrameData& data) {
    Renderer& renderer = *engine.renderer;
    CFDSolver& solver = cfd.solver;
    CFDVisualization& visualization = *cfd.visualization;
    CFDSceneRenderData& cfd_render_data = cfd.render_data;
    
    RenderPass screen = begin_render_frame();

    //RENDER SCENE
    {
        RenderPass render_pass = begin_cfd_scene_pass(visualization, renderer, data);
        CommandBuffer& cmd_buffer = render_pass.cmd_buffer;

        if (solver.phase > SOLVER_PHASE_MESH_GENERATION) {
            render_cfd_mesh(visualization, cmd_buffer);
        }

        render_cfd_scene(visualization, cfd_render_data, cmd_buffer);
        end_render_pass(render_pass);
    }
    
    submit_draw_data(*cfd.ui_renderer, screen.cmd_buffer, get_ui_draw_data(*cfd.ui));
    
    end_render_frame(screen);
}

APPLICATION_API void deinit(CFD* cfd, Modules& engine) {
    destroy_UI(cfd->ui);
	delete cfd;
}
