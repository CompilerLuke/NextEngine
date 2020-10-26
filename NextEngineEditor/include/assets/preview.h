#pragma once

#include <engine/handle.h>
#include <graphics/rhi/buffer.h>
#include <graphics/rhi/frame_buffer.h>
#include "info.h"

struct RotatablePreview;
struct Camera;

struct AssetPreviewResources {
	pipeline_layout_handle pipeline_layout;
	descriptor_set_handle pass_descriptor[MAX_FRAMES_IN_FLIGHT];
	descriptor_set_handle pbr_descriptor;
	UBOBuffer pass_ubo[MAX_FRAMES_IN_FLIGHT];
	UBOBuffer simulation_ubo[MAX_FRAMES_IN_FLIGHT];
	Framebuffer preview_fbo;
	Framebuffer preview_tonemapped_fbo;
	texture_handle preview_map;
	texture_handle preview_tonemapped_map;
	texture_handle preview_atlas;
	bool atlas_layout_undefined = true;
	array<MAX_ASSETS, glm::vec2> free_atlas_slot;
	vector<asset_handle> render_preview_for;
};

const RenderPass::ID PreviewPass = (RenderPass::ID)(RenderPass::PassCount + 0);
const RenderPass::ID PreviewPassTonemap = (RenderPass::ID)(RenderPass::PassCount + 1);

void rot_preview(AssetPreviewResources& resources, RotatablePreview& self);
void preview_from_atlas(AssetPreviewResources& resources, RotatablePreview& preview, texture_handle* handle, glm::vec2* uv0, glm::vec2* uv1);
void preview_image(AssetPreviewResources& resources, RotatablePreview& preview, glm::vec2 size);

RenderPass begin_preview_pass(AssetPreviewResources& self, Camera& camera, Transform& trans);
void end_preview_pass(AssetPreviewResources& self, RenderPass& render_pass, glm::vec2 copy_preview_to);

void render_preview_for(AssetPreviewResources& self, MaterialAsset& asset);
void render_preview_for(AssetPreviewResources& self, ModelAsset& asset);