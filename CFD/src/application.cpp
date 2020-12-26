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

struct CFD {
	bool initialized = false;
	CFDSolver solver;
	CFDVisualization visualization;
    UI* ui;
    UIRenderer* ui_renderer;
    texture_handle scene;
    texture_handle camera;
    texture_handle gizmo;
};

constexpr glm::vec4 shade1 = color4(30, 30, 30, 1);
constexpr glm::vec4 shade2 = color4(40, 40, 40, 1);
constexpr glm::vec4 shade3 = color4(50, 50, 50, 1);
constexpr glm::vec4 border = color4(100, 100, 100, 1);
constexpr glm::vec4 blue = color4(52, 159, 235, 1);
constexpr glm::vec4 white = color4(255,255,255,1);


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
    .size(ThemeSize::DefaultFontSize, 12)
    .size(ThemeSize::Title1FontSize, 18)
    .size(ThemeSize::Title2FontSize, 16)
    .size(ThemeSize::Title3FontSize, 14)
    .size(ThemeSize::SplitterThickness, 1)
    .size(ThemeSize::StackMargin, 0)
    .size(ThemeSize::StackPadding, 2.5)
    .size(ThemeSize::StackSpacing, 2.5)
    .size(ThemeSize::VecSpacing, 2.5)
    .size(ThemeSize::TextPadding, 0)
    .size(ThemeSize::InputPadding, 5)
    .size(ThemeSize::InputMinWidth, 60)
    .size(ThemeSize::InputMaxWidth, {Perc, 25});
}


APPLICATION_API CFD* init(void* args, Modules& engine) {
	CFD* cfd = new CFD();
    cfd->ui = make_UI();
    cfd->ui_renderer = make_ui_renderer();
    
    set_theme(get_ui_theme(*cfd->ui));
    
    load_font(*cfd->ui, "editor/fonts/segoeui.ttf");
    
    World& world = *engine.world;
    
    auto [e,trans,camera] = world.make<Transform, Camera>();
    
    Dependency dependencies[1] = {
        { FRAGMENT_STAGE, RenderPass::Scene },
    };

    build_framegraph(*engine.renderer, { dependencies, 1});
    
    
    cfd->scene = load_Texture("scene.png");
    cfd->gizmo = load_Texture("gizmo.png");
    cfd->camera = load_Texture("camera.png");
    
    end_gpu_upload();

    
	return cfd;
}

APPLICATION_API bool is_running(CFD& cfd, Modules& modules) {
	return !modules.window->should_close();
}

APPLICATION_API void enter_play_mode(CFD& cfd, Modules& modules) {
	if (!cfd.initialized) {
		make_cfd_visualization(cfd.visualization);
		cfd.initialized = true;
	}
	cfd.solver.phase = SOLVER_PHASE_NONE;
}

APPLICATION_API void update(CFD& cfd, Modules& modules) {
	World& world = *modules.world;
	CFDSolver& solver = cfd.solver;

	UpdateCtx update_ctx(*modules.time, *modules.input);
	update_ctx.layermask = EntityQuery().with_none(EDITOR_ONLY);
	update_flyover(world, update_ctx);

	if (solver.phase == SOLVER_PHASE_NONE) {
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

			build_vertex_representation(cfd.visualization, solver.mesh);
		}
	}
}

void nav_bar_li(UI& ui, string_view name) {
    text(ui, name)
    .font(12)
    .color(WHITE);
}

template<typename T>
void field(UI& ui,string_view field, T* value, float min = -FLT_MAX, float max = FLT_MAX, float inc_per_pixel = 5.0) {
    begin_hstack(ui).padding(0);
    text(ui, field); //.color(color4(220,220,220));
    spacer(ui);
    input(ui, value, min, max, inc_per_pixel);
    end_hstack(ui);
}

void divider(UI& ui, Color color) {
    begin_vstack(ui).background(color).width({Perc,100}).height(1);
    end_vstack(ui);
}

void begin_component(UI& ui, const char* title, texture_handle icon) {
    begin_vstack(ui).background(shade2);

    begin_hstack(ui).padding(0);
        text(ui, title);
        spacer(ui);
        text(ui, "X");
    end_hstack(ui);
}

void end_component(UI& ui) {
    end_vstack(ui);
    divider(ui, shade1);
}

APPLICATION_API void extract_render_data(CFD& cfd, Modules& engine, FrameData& data) {
    Viewport viewport = {};
    viewport.width = engine.window->width;
    viewport.height = engine.window->height;
    
    World& world = *engine.world;
    
    //extract_render_data(*engine.renderer, viewport, data, world, EntityQuery(), EntityQuery());
    
    UI& ui = *cfd.ui;
    
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

    begin_vstack(ui,0)
        .width({Perc,100})
        .height({Perc,100})
        .padding(0)
        .background(shade2);
    
    StackView& stack = begin_hstack(ui, 20)
        .width({Perc,100})
        .background(shade1);
    {
        text(ui, "File");
        text(ui, "Edit");
        text(ui, "Settings");
        spacer(ui);
        text(ui, "X");
    }
    end_hstack(ui);
    divider(ui, shade1);
    
    begin_hsplitter(ui, "right").width({Perc,100}).flex(glm::vec2(0,1));
    {
        begin_vstack(ui,0).height({Perc,100}).padding(0);
        {
            begin_vstack(ui);
            text(ui, "Properties");
            begin_hstack(ui, HCenter).padding(0);
            static char name[100] = "Windmill";
            button(ui, "Static").padding(2.5);
            input(ui, name, 100).flex(glm::vec2(1,0));
            end_hstack(ui);
            
            end_vstack(ui);
            
            divider(ui, shade1);
            
            static Transform trans;
            static Camera camera;
            
            begin_component(ui, "Transform", cfd.gizmo);
            {
                field(ui, "position", &trans.position);
                field(ui, "scale", &trans.scale);
            }
            end_component(ui);
            begin_component(ui, "Camera", cfd.camera);
            {
                field(ui, "fov", &camera.fov, 0, 180);
                field(ui, "near", &camera.near_plane);
                field(ui, "far", &camera.far_plane);
            }
            end_component(ui);
        }
        
        
        spacer(ui);
        divider(ui, shade1);
        button(ui, "Add Component");
        end_vstack(ui);
        
        begin_vsplitter(ui, "Scene");
        {
            image(ui, cfd.scene)
                    .scale_to_fill();
            
            begin_vstack(ui);
            {
                title1(ui, "Assets");
            }
            end_vstack(ui);
        }
        end_vsplitter(ui);
    }
    end_hsplitter(ui);
    
    
    end_vstack(ui);
    
    end_ui_frame(ui);
    
    engine.window->set_cursor(get_ui_cursor_shape(ui));
}



APPLICATION_API void render(CFD& cfd, Modules& engine, GPUSubmission& _gpu_submission, FrameData& data) {
    Renderer& renderer = *engine.renderer;
    
    RenderPass screen = begin_render_frame();
    
    //RenderPass& scene = gpu_submission.render_passes[RenderPass::Scene];
	
    CFDSolver& solver = cfd.solver;
	CFDVisualization& visualization = cfd.visualization;

	if (solver.phase > SOLVER_PHASE_MESH_GENERATION) {
		//render_cfd_mesh(visualization, scene.cmd_buffer);
	}
    
    submit_draw_data(*cfd.ui_renderer, screen.cmd_buffer, get_ui_draw_data(*cfd.ui));
    
    end_render_frame(screen);
}

APPLICATION_API void deinit(CFD* cfd, Modules& engine) {
    destroy_UI(cfd->ui);
	delete cfd;
}
