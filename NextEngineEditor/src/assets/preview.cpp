#include "assets/preview.h"
#include <imgui/imgui.h>
#include <graphics/rhi/rhi.h>
#include <graphics/renderer/renderer.h>
#include <components/camera.h>

const uint ATLAS_PREVIEWS_WIDTH = 10;
const uint ATLAS_PREVIEWS_HEIGHT = 10;

const uint ATLAS_PREVIEW_RESOLUTION = 128;


void preview_from_atlas(AssetPreviewResources& resources, RotatablePreview& preview, texture_handle* handle, glm::vec2* uv0, glm::vec2* uv1) {
	*uv0 = preview.preview_tex_coord;
	*uv1 = *uv0 + glm::vec2(1.0 / ATLAS_PREVIEWS_WIDTH, 1.0 / ATLAS_PREVIEWS_HEIGHT);
	*handle = resources.preview_atlas;
}

void preview_image(AssetPreviewResources& resources, RotatablePreview& preview, glm::vec2 size) {
	texture_handle preview_atlas;
	glm::vec2 uv0, uv1;

	preview_from_atlas(resources, preview, &preview_atlas, &uv0, &uv1);
	ImGui::Image(preview_atlas, size, uv0, uv1);
}


RenderPass begin_preview_pass(AssetPreviewResources& self, Camera& camera, Transform& trans) {
	RenderPass render_pass = begin_render_pass(PreviewPass, glm::vec4(0.2, 0.2, 0.2, 1.0));

	uint frame_index = get_frame_index();

	bind_vertex_buffer(render_pass.cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);
	bind_pipeline_layout(render_pass.cmd_buffer, self.pipeline_layout);
	bind_descriptor(render_pass.cmd_buffer, 0, self.pass_descriptor[frame_index]);
	bind_descriptor(render_pass.cmd_buffer, 1, self.pbr_descriptor);

	update_camera_matrices(trans, camera, render_pass.viewport);

	//todo create function
	SimulationUBO simulation_ubo;
	simulation_ubo.time = Time::now();
	memcpy_ubo_buffer(self.simulation_ubo[frame_index], &simulation_ubo);

	PassUBO pass_ubo;
	fill_pass_ubo(pass_ubo, render_pass.viewport);
	memcpy_ubo_buffer(self.pass_ubo[frame_index], &pass_ubo);

	return render_pass;
}

void end_preview_pass(AssetPreviewResources& self, RenderPass& render_pass, glm::vec2 copy_preview_to) {
	end_render_pass(render_pass);

	ImageOffset src_region[2] = { 0,0,render_pass.viewport.width, render_pass.viewport.height };
	ImageOffset dst_region[2];
	dst_region[0].x = copy_preview_to.x * ATLAS_PREVIEWS_WIDTH * ATLAS_PREVIEW_RESOLUTION;
	dst_region[0].y = copy_preview_to.y * ATLAS_PREVIEWS_HEIGHT * ATLAS_PREVIEW_RESOLUTION;
	dst_region[1].x = dst_region[0].x + ATLAS_PREVIEW_RESOLUTION;
	dst_region[1].y = dst_region[0].y + ATLAS_PREVIEW_RESOLUTION;

	if (self.atlas_layout_undefined) {
		transition_layout(render_pass.cmd_buffer, self.preview_atlas, TextureLayout::Undefined, TextureLayout::TransferDstOptimal);
		self.atlas_layout_undefined = false;
	}
	else {
		transition_layout(render_pass.cmd_buffer, self.preview_atlas, TextureLayout::ShaderReadOptimal, TextureLayout::TransferDstOptimal);
	}

	transition_layout(render_pass.cmd_buffer, self.preview_map, TextureLayout::ColorAttachmentOptimal, TextureLayout::TransferSrcOptimal);
	blit_image(render_pass.cmd_buffer, Filter::Linear, self.preview_map, src_region, self.preview_atlas, dst_region);
	transition_layout(render_pass.cmd_buffer, self.preview_atlas, TextureLayout::TransferDstOptimal, TextureLayout::ShaderReadOptimal);
	transition_layout(render_pass.cmd_buffer, self.preview_map, TextureLayout::TransferSrcOptimal, TextureLayout::ShaderReadOptimal);
}

void render_preview_for(AssetPreviewResources& self, ModelAsset& asset) {
	Camera camera;
	Transform camera_trans, trans_of_model;
	trans_of_model.rotation = asset.rot_preview.rot;

	Model* model = get_Model(asset.handle);
	AABB aabb = model->aabb;

	glm::vec3 center = (aabb.max + aabb.min) / 2.0f;
	float radius = 0;

	glm::vec4 verts[8];
	aabb.to_verts(verts);
	for (int i = 0; i < 8; i++) {
		radius = glm::max(radius, glm::length(glm::vec3(verts[i]) - center));
	}

	double fov = glm::radians(60.0f);

	camera.fov = 60.0f; // fov;
	camera_trans.position = center;
	camera_trans.position.z += (radius) / glm::tan(fov / 2.0);

	RenderPass render_pass = begin_preview_pass(self, camera, camera_trans);

	draw_mesh(render_pass.cmd_buffer, asset.handle, asset.materials, trans_of_model);

	end_preview_pass(self, render_pass, asset.rot_preview.preview_tex_coord);
}


void render_preview_for(AssetPreviewResources& self, MaterialAsset& asset) {
	Camera camera;
	Transform camera_trans;
	camera_trans.position.z = 2.5;

	Transform trans_of_sphere;
	trans_of_sphere.rotation = asset.rot_preview.rot;

	RenderPass render_pass = begin_preview_pass(self, camera, camera_trans);

	draw_mesh(render_pass.cmd_buffer, primitives.sphere, asset.handle, trans_of_sphere);

	end_preview_pass(self, render_pass, asset.rot_preview.preview_tex_coord);
}

void rot_preview(AssetPreviewResources& resources, RotatablePreview& self) {
	ImGui::SetNextTreeNodeOpen(true);
	ImGui::CollapsingHeader("Preview");

	ImGui::Image(resources.preview_map, ImVec2(512, 512));

	if (ImGui::IsItemHovered() && ImGui::IsMouseDragging()) {
		self.previous = self.current;
		self.current = glm::vec2(ImGui::GetMouseDragDelta().x, ImGui::GetMouseDragDelta().y);

		glm::vec2 diff = self.current - self.previous;

		self.rot_deg += diff;
		self.rot = glm::quat(glm::vec3(glm::radians(self.rot_deg.y), 0, 0)) * glm::quat(glm::vec3(0, glm::radians(self.rot_deg.x), 0));
	}
	else {
		self.previous = glm::vec2(0);
	}

	//todo copy preview map into preview atlas
}



void generate_free_atlas_slots(array<MAX_ASSETS, glm::vec2>& free_atlas_slots) {
	for (uint h = 0; h < ATLAS_PREVIEWS_HEIGHT; h++) {
		for (uint w = 0; w < ATLAS_PREVIEWS_WIDTH; w++) {
			glm::vec2 tex_coord((float)w / ATLAS_PREVIEWS_WIDTH, (float)h / ATLAS_PREVIEWS_HEIGHT);
			free_atlas_slots.append(tex_coord);
		}
	}
}

void make_AssetPreviewRenderData(AssetPreviewResources& resources, Renderer& renderer) {
	resources.pbr_descriptor = renderer.lighting_system.pbr_descriptor;

	{
		FramebufferDesc desc{ 512, 512 };
		AttachmentDesc& color_attachment = add_color_attachment(desc, &resources.preview_map);
		color_attachment.final_layout = TextureLayout::ColorAttachmentOptimal;
		color_attachment.usage |= TextureUsage::TransferSrc;


		make_Framebuffer(PreviewPass, desc);
	}

	{
		FramebufferDesc desc{ 512, 512 };
		add_color_attachment(desc, &resources.preview_tonemapped_map);
		make_Framebuffer(PreviewPassTonemap, desc);
	}

	for (uint i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		resources.pass_ubo[i] = alloc_ubo_buffer(sizeof(PassUBO), UBO_PERMANENT_MAP);
		resources.simulation_ubo[i] = alloc_ubo_buffer(sizeof(SimulationUBO), UBO_PERMANENT_MAP);
		//todo wouldn't the renderer simulation ubo suffice, no need to allocate a second one

		DescriptorDesc descriptor_desc = {};
		add_ubo(descriptor_desc, VERTEX_STAGE, resources.pass_ubo[i], 0);
		add_ubo(descriptor_desc, VERTEX_STAGE | FRAGMENT_STAGE, resources.simulation_ubo[i], 1);
		update_descriptor_set(resources.pass_descriptor[i], descriptor_desc);
	}

	array<2, descriptor_set_handle> descriptor_layouts = { resources.pass_descriptor[0], resources.pbr_descriptor };
	resources.pipeline_layout = query_Layout(descriptor_layouts);

	generate_free_atlas_slots(resources.free_atlas_slot);

	TextureDesc desc = {};
	desc.width = ATLAS_PREVIEWS_WIDTH * ATLAS_PREVIEW_RESOLUTION;
	desc.height = ATLAS_PREVIEWS_HEIGHT * ATLAS_PREVIEW_RESOLUTION;
	desc.num_channels = 4;

	resources.preview_atlas = alloc_Texture(desc);
}

//todo although it is possible to render multiple previews in one frame
//this would require several ubo buffers, possibly with push constants to get the right one!
//in theory this also has a frame sychronization issue, as it's not using multiple ubo for different frames
void render_previews(AssetPreviewResources& self, AssetInfo& info) {
	while (self.render_preview_for.length > 0) {
		AssetNode* node = get_asset(info, self.render_preview_for.pop());

		if (node->type == AssetNode::Material) render_preview_for(self, node->material);
		if (node->type == AssetNode::Model) render_preview_for(self, node->model);

		break;
	}
}

void alloc_atlas_slot(AssetPreviewResources& resources, RotatablePreview& preview) {
	preview.preview_tex_coord = resources.free_atlas_slot.pop();
}

void free_atlas_slot(AssetPreviewResources& resources, RotatablePreview& preview) {
	resources.free_atlas_slot.append(preview.preview_tex_coord);
}