#pragma once

#include "engine/handle.h"
#include "core/container/tvector.h"
#include "graphics/rhi/buffer.h"
#include "ecs/id.h"
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

struct CommandBuffer;
struct CFDVolume;
struct World;
struct CFDVisualization;
struct RenderPass;
struct FrameData;
struct Renderer;

CFDVisualization* make_cfd_visualization();
void build_vertex_representation(CFDVisualization& visualization, CFDVolume&);
void render_cfd_mesh(CFDVisualization& visualization, CommandBuffer& cmd_buffer);

struct CFDMeshInstance {
	glm::mat4 model;
	VertexBuffer vertex;
	glm::vec4 color;
};

struct CFDSceneRenderData {
	tvector<CFDMeshInstance> instances;
};

RenderPass begin_cfd_scene_pass(CFDVisualization& visualization, Renderer& renderer, FrameData& frame);
void extract_cfd_scene_render_data(CFDSceneRenderData& render_data, World& world, EntityQuery query);
void render_cfd_scene(CFDVisualization&, const CFDSceneRenderData, CommandBuffer&);