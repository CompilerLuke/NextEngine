#include "engine/engine.h"
#include "engine/application.h"
#include "graphics/rhi/window.h"
#include "graphics/renderer/renderer.h"
#include "ecs/ecs.h"
#include "components/transform.h"
#include "components/flyover.h"

#include "visualizer.h"

#include "solver.h"
#include "mesh.h"
#include "components.h"

struct CFD {
	bool initialized = false;
	CFDSolver solver;
	CFDVisualization visualization;
};

APPLICATION_API CFD* init(Modules& engine) {
	CFD* cfd = new CFD();
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

APPLICATION_API void extract_render_data(CFD& cfd, Modules& engine, FrameData& data) {

}


APPLICATION_API void render(CFD& cfd, Modules& engine, GPUSubmission& gpu_submission, FrameData& data) {
	RenderPass& scene = gpu_submission.render_passes[RenderPass::Scene];
	CFDSolver& solver = cfd.solver;
	CFDVisualization& visualization = cfd.visualization;

	if (solver.phase > SOLVER_PHASE_MESH_GENERATION) {
		render_cfd_mesh(visualization, scene.cmd_buffer);
	}
}

APPLICATION_API void deinit(CFD* cfd, Modules& engine) {
	delete cfd;
}
