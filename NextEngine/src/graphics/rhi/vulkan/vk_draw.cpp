#include "graphics/rhi/draw.h"
#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/pipeline.h"
#include "graphics/rhi/vulkan/buffer.h"
#include "graphics/rhi/vulkan/command_buffer.h"
#include "graphics/rhi/vulkan/draw.h"
#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/material.h"
#include "graphics/pass/pass.h"
#include "graphics/renderer/renderer.h"
#include "graphics/assets/material.h"
#include "graphics/assets/model.h"
#include "graphics/assets/assets.h"
#include "components/transform.h"

#ifdef RENDER_API_VULKAN

//A command buffer's lifetime should never exceed that of a frame

CommandBuffer& begin_draw_cmds() {
	CommandBuffer& cmd_buffer = *TEMPORARY_ALLOC(CommandBuffer);
	cmd_buffer = {};
	cmd_buffer.cmd_buffer = begin_recording(render_thread.command_pool);

	return cmd_buffer;
}

void end_draw_cmds(CommandBuffer& cmd_buffer) {
	end_recording(render_thread.command_pool, cmd_buffer.cmd_buffer);
	cmd_buffer.cmd_buffer = VK_NULL_HANDLE;
}

void draw_mesh(CommandBuffer& cmd_buffer, model_handle model_handle, slice<material_handle> materials, Transform& trans, uint lod) {
	glm::mat4 model_m = compute_model_matrix(trans);
	draw_mesh(cmd_buffer, model_handle, materials, model_m, lod);
}

void draw_mesh(CommandBuffer& cmd_buffer, model_handle model_handle, slice<material_handle> materials, glm::mat4 trans, uint lod) {
	slice<glm::mat4> model_m = trans;
	draw_mesh(cmd_buffer, model_handle, materials, model_m, lod);
}

void draw_mesh(CommandBuffer& cmd_buffer, model_handle model_handle, slice<material_handle> materials, slice<glm::mat4> model_m, uint lod) {
	bind_vertex_buffer(cmd_buffer, VERTEX_LAYOUT_DEFAULT, INSTANCE_LAYOUT_MAT4X4);
	
	InstanceBuffer instance_buffer = frame_alloc_instance_buffer<glm::mat4>(INSTANCE_LAYOUT_MAT4X4, model_m);
	
	Model* model = get_Model(model_handle);

	for (Mesh& mesh : model->meshes) {
		material_handle mat_handle = materials[mesh.material_id];
		pipeline_handle pipeline_handle = query_pipeline(mat_handle, cmd_buffer.render_pass, cmd_buffer.subpass); 

		bind_pipeline(cmd_buffer, pipeline_handle);
		bind_material(cmd_buffer, mat_handle);
		draw_mesh(cmd_buffer, mesh.buffer[lod], instance_buffer);
	}
}

void bind_descriptor(CommandBuffer& cmd_buffer, uint binding, slice<descriptor_set_handle> sets) {
	VkDescriptorSet vk_sets[MAX_SET];
	VkPipelineLayout pipeline_layout = get_pipeline_layout(rhi.pipeline_cache, cmd_buffer.bound_pipeline_layout);
	
	for (uint i = 0; i < sets.length; i++) {
		vk_sets[i] = get_descriptor_set(sets[i]);
	}

	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, binding, sets.length, vk_sets, 0, nullptr);
}

void bind_vertex_buffer(CommandBuffer& cmd_buffer, VertexLayout vertex_layout, InstanceLayout instance_layout) {		
	if (cmd_buffer.bound_vertex_layout != vertex_layout) {
		bind_vertex_buffer(rhi.vertex_streaming, cmd_buffer.cmd_buffer, vertex_layout);		
		cmd_buffer.bound_vertex_layout = vertex_layout;
	}

	if (cmd_buffer.bound_instance_layout != instance_layout) {
		bind_instance_buffer(render_thread.instance_allocator, cmd_buffer.cmd_buffer, instance_layout);
		cmd_buffer.bound_instance_layout = instance_layout;
	}
}

void bind_pipeline_layout(CommandBuffer& cmd_buffer, pipeline_layout_handle pipeline_layout_handle) {
	cmd_buffer.bound_pipeline_layout = pipeline_layout_handle;
	cmd_buffer.bound_pipeline = { INVALID_HANDLE };
}

void bind_pipeline(CommandBuffer& cmd_buffer, pipeline_handle pipeline_handle) {	
	if (cmd_buffer.bound_pipeline.id == pipeline_handle.id) return;

	VkPipeline pipeline = get_Pipeline(rhi.pipeline_cache, pipeline_handle);
	vkCmdBindPipeline(cmd_buffer.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	
	cmd_buffer.bound_pipeline = pipeline_handle;
	cmd_buffer.bound_pipeline_layout = { (u64)get_pipeline_layout(rhi.pipeline_cache, pipeline_handle) };
	cmd_buffer.bound_material = { INVALID_HANDLE };
}

void bind_material(CommandBuffer& cmd_buffer, material_handle mat_handle) {
	if (cmd_buffer.bound_material.id == mat_handle.id) return;
	cmd_buffer.bound_material = mat_handle;

	Material* mat = get_Material(mat_handle);

	descriptor_set_handle set_handle = mat->sets[mat->index];
	VkDescriptorSet set = get_descriptor_set(set_handle);
	VkPipelineLayout pipeline_layout = get_pipeline_layout(rhi.pipeline_cache, cmd_buffer.bound_pipeline_layout);

	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, MATERIAL_SET, 1, &set, 0, 0);
}

void draw_mesh(CommandBuffer& cmd_buffer, VertexBuffer vertex_buffer, InstanceBuffer instance_buffer) {	
	vkCmdDrawIndexed(cmd_buffer.cmd_buffer, vertex_buffer.length, instance_buffer.length, vertex_buffer.index_base, vertex_buffer.vertex_base, instance_buffer.base);
}

void draw_mesh(CommandBuffer& cmd_buffer, VertexBuffer vertex_buffer) {
	vkCmdDrawIndexed(cmd_buffer.cmd_buffer, vertex_buffer.length, 1, vertex_buffer.index_base, vertex_buffer.vertex_base, 0);
}

void push_constant(CommandBuffer& cmd_buffer, Stage stage, uint offset, uint size, const void* ptr) {
	VkPipelineLayout pipeline_layout = get_pipeline_layout(rhi.pipeline_cache, cmd_buffer.bound_pipeline_layout);

	vkCmdPushConstants(cmd_buffer, pipeline_layout, to_vk_stage(stage), offset, size, ptr);
}

#endif