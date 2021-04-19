#pragma once

#include "core/container/tvector.h"
#include "ecs/id.h"
#include "engine/handle.h"
#include <glm/vec4.hpp>
#include "cfd_components.h"
#include "visualization/render_backend.h"

struct SceneViewport;
struct Input;
struct InputMeshRegistry;
struct World;
struct CommandBuffer;
struct CFDMeshInstance;
struct Viewport;

class InputMeshViewer {
	struct RenderData {
		tvector<CFDMeshInstance> instances;
	};
	
	InputMeshRegistry& registry;
	World& world;
	SceneViewport& viewport;
	CFDRenderBackend& backend;

	glm::vec4 line_color = { 0,0,0,1.0 };

	CFDTriangleBuffer triangles[MAX_FRAMES_IN_FLIGHT];
	CFDLineBuffer lines[MAX_FRAMES_IN_FLIGHT];

	pipeline_handle pipeline_solid;
	pipeline_handle pipeline_wireframe;
	
	RenderData render_data;

public:
	InputMeshViewer(World&, CFDRenderBackend& backend, InputMeshRegistry&, SceneViewport&);
	
	void extract_render_data(EntityQuery query);
	void render(CommandBuffer&, descriptor_set_handle frame_descriptor);
};