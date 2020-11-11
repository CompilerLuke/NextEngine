// dear imgui: Renderer for Vulkan with shaders / programmatic pipeline
// - Desktop Vulkan: 3.x 4.x
// This needs to be used along with a Platform Binding (e.g. GLFW, SDL, Win32, custom..)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'GLuint' OpenGL texture identifier as void*/ImTextureID. Read the FAQ about ImTextureID in imgui.cpp.
//  [X] Renderer: Multi-viewport support. Enable with 'io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable'.

// Note: not the renderer officially provided with ImGui

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include <stdio.h>

#include <stdint.h>     
#include <GLFW/glfw3.h>
#include "graphics/rhi/vulkan/vulkan.h"
#include "graphics/rhi/vulkan/draw.h"
#include "graphics/rhi/rhi.h"
#include "engine/vfs.h"
#include "core/container/hash_map.h"
#include "graphics/assets/assets.h"

const uint MAX_IMGUI_TEXTURES = 16;
const u64 MAX_IMGUI_VERTEX_BUFFER_SIZE = mb(1);
const u64 MAX_IMGUI_INDEX_BUFFER_SIZE = mb(1);

struct ImGui_Proj {
	float proj_mtx[4][4];
};

struct ImGuiVulkan_Backend {
	texture_handle font_texture;
	HostVisibleBuffer vertex_buffer;
	HostVisibleBuffer index_buffer;
	Arena vertex_arena[MAX_FRAMES_IN_FLIGHT];
	Arena index_arena[MAX_FRAMES_IN_FLIGHT];
	UBOBuffer proj_ubo_buffer;
	descriptor_set_handle descriptor[MAX_FRAMES_IN_FLIGHT];
	sampler_handle sampler;
	texture_handle dummy_tex;
	VkPipeline pipeline;
	VkPipelineLayout pipeline_layout;
	VkShaderModule vert_shader;
	VkShaderModule frag_shader;
};

static ImGuiVulkan_Backend backend;

// Forward Declarations
static void ImGui_ImplVulkan_InitPlatformInterface();
static void ImGui_ImplVulkan_ShutdownPlatformInterface();

// Functions
bool    ImGui_ImplVulkan_Init(const char* glsl_version)
{
	// Setup back-end capabilities flags
	ImGuiIO& io = ImGui::GetIO();
	//io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;    // We can create multi-viewports on the Renderer side (optional)
	io.BackendRendererName = "imgui_impl_vulkan";

	// Store GLSL version string so we can refer to it later in case we recreate shaders. Note: GLSL version is NOT the same as GL version. Leave this to NULL if unsure.

	//IM_ASSERT((int)strlen(glsl_version) + 2 < IM_ARRAYSIZE(g_GlslVersionString));
	//strcpy(g_GlslVersionString, glsl_version);
	//strcat(g_GlslVersionString, "\n");

	// Make a dummy GL call (we don't actually need the result)
	// IF YOU GET A CRASH HERE: it probably means that you haven't initialized the OpenGL function loader used by this code.
	// Desktop OpenGL 3/4 need a function loader. See the IMGUI_IMPL_OPENGL_LOADER_xxx explanation above.
	//GLint current_texture;
	//glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_texture);

	//if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) ImGui_ImplVulkan_InitPlatformInterface();

	return true;
}

void    ImGui_ImplVulkan_Shutdown()
{
	ImGui_ImplVulkan_ShutdownPlatformInterface();
	ImGui_ImplVulkan_DestroyDeviceObjects();
}

void    ImGui_ImplVulkan_NewFrame()
{
	if (backend.font_texture.id == INVALID_HANDLE)
	    ImGui_ImplVulkan_CreateDeviceObjects();
}

#include "core/profiler.h"

// Vulkan Render function.
void ImGui_ImplVulkan_RenderDrawData(CommandBuffer& draw_cmd_buffer, ImDrawData* draw_data) {
	//ensure state is reset
	draw_cmd_buffer.bound_vertex_layout = (VertexLayout)-1;
	draw_cmd_buffer.bound_instance_layout = (InstanceLayout)-1;
	draw_cmd_buffer.bound_material = { INVALID_HANDLE };
	draw_cmd_buffer.bound_pipeline = { INVALID_HANDLE };
	
	VkCommandBuffer cmd_buffer = draw_cmd_buffer;

	assert(sizeof(ImDrawIdx) == 2 || sizeof(ImDrawIdx) == 4);

	// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
	int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
	int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
	if (fb_width <= 0 || fb_height <= 0) return;


	bool clip_origin_lower_left = true;

	// Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, polygon fill	 
	
	vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, backend.pipeline);

	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = fb_width;
	viewport.height = fb_height;

	vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

	 // Setup viewport, orthographic projection matrix
	 // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayMin is (0,0) for single viewport apps.

	float L = draw_data->DisplayPos.x;
	float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
	float T = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
	float B = draw_data->DisplayPos.y;
	
	float ortho_projection[4][4] =
	{
		{ 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
		{ 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
		{ 0.0f,         0.0f,        -1.0f,   0.0f },
		{ (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
	};


	//ortho_projection[1][1] *= -1;

	memcpy_ubo_buffer(backend.proj_ubo_buffer, &ortho_projection);

	ImDrawVert* vertex_buffer = (ImDrawVert*)backend.vertex_buffer.mapped;
	uint* index_buffer = (uint*)backend.index_buffer.mapped;

	u64 vertex_offset = backend.vertex_arena[rhi.frame_index].base_offset;
	u64 index_offset = backend.index_arena[rhi.frame_index].base_offset;
	u64 vertices_rendered = 0;

	vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &backend.vertex_buffer.buffer, &vertex_offset);
	vkCmdBindIndexBuffer(cmd_buffer, backend.index_buffer.buffer, index_offset, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);

	// Will project scissor/clipping rectangles into framebuffer space
	ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
	ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

	hash_map<uint, CombinedSampler, MAX_IMGUI_TEXTURES> combined_samplers = {};
	
	for (uint i = 0; i < MAX_IMGUI_TEXTURES; i++) {
		combined_samplers.values[i].sampler = backend.sampler;
		combined_samplers.values[i].texture = backend.dummy_tex;
	}

	// Upload textures
	for (int n = 0; n < draw_data->CmdListsCount; n++) {
		const ImDrawList* cmd_list = draw_data->CmdLists[n];

		for (const ImDrawCmd& pcmd : cmd_list->CmdBuffer) {
			if (!pcmd.UserCallback) {
				texture_handle id = { (uint)(u64)pcmd.TextureId };

				CombinedSampler combined_sampler = {};
				combined_sampler.texture = id;
				combined_sampler.sampler = backend.sampler;

				combined_samplers.set(id.id, combined_sampler);
			}
		}
	}

	{
		DescriptorDesc descriptor_desc;
		add_combined_sampler(descriptor_desc, FRAGMENT_STAGE, { combined_samplers.values, MAX_IMGUI_TEXTURES }, 1);
		update_descriptor_set(backend.descriptor[rhi.frame_index], descriptor_desc);

		VkDescriptorSet descriptor = get_descriptor_set(backend.descriptor[rhi.frame_index]);
		vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, backend.pipeline_layout, 0, 1, &descriptor, 0, 0);
	}

	size_t idx_buffer_offset = 0;

	// Render command lists
	for (int n = 0; n < draw_data->CmdListsCount; n++) {
		const ImDrawList* cmd_list = draw_data->CmdLists[n];

		u64 vertex_size = cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
		u64 index_size = cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);

		memcpy((char*)vertex_buffer + vertex_offset, cmd_list->VtxBuffer.Data, vertex_size);
		memcpy((char*)index_buffer + index_offset, cmd_list->IdxBuffer.Data, index_size);

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback)
			{
				// User callback (registered via ImDrawList::AddCallback)
				pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				// Project scissor/clipping rectangles into framebuffer space
				ImVec4 clip_rect;
				clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
				clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
				clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
				clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

				if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f)
				{
					VkRect2D scissor;
					scissor.offset.x = clip_rect.x;
					scissor.offset.y = clip_rect.y; 
					scissor.extent.width = clip_rect.z - clip_rect.x;
					scissor.extent.height = clip_rect.w - clip_rect.y; 

					int tex_id = combined_samplers.keys.index((u64)pcmd->TextureId);

					vkCmdPushConstants(cmd_buffer, backend.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int), &tex_id);
					vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);
					vkCmdDrawIndexed(cmd_buffer, pcmd->ElemCount, 1, idx_buffer_offset, vertices_rendered, 0);
				}
			}
			idx_buffer_offset += pcmd->ElemCount;
		}

		vertex_offset += vertex_size;
		index_offset += index_size;
		vertices_rendered += cmd_list->VtxBuffer.Size;
	}
}

#ifdef IMGUI_FREETYPE
#include <imgui/misc/freetype/imgui_freetype.h>
#endif

bool ImGui_ImplVulkan_CreateFontsTexture()
{
	//Build texture atlas
	
	ImGuiIO& io = ImGui::GetIO();
	
	// Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.
	Image image = {};
	image.num_channels = 4;
	image.format = TextureFormat::UNORM;
    
    int width, height;
    unsigned int flags = ImGuiFreeType::NoHinting;
#ifdef IMGUI_FREETYPE
    ImGuiFreeType::BuildFontAtlas(io.Fonts, flags);
#endif
	io.Fonts->GetTexDataAsRGBA32((unsigned char**)&image.data, &width, &height);

    image.width = width;
    image.height = height;
    
	backend.font_texture = upload_Texture(image);
	io.Fonts->TexID = (ImTextureID)backend.font_texture.id;

	return true;
}

void ImGui_ImplVulkan_DestroyFontsTexture() {
	if (backend.font_texture.id != INVALID_HANDLE) {
		//todo actually delete texture
		ImGuiIO& io = ImGui::GetIO();
		//glDeleteTextures(1, &g_FontTexture);
		io.Fonts->TexID = 0;
		backend.font_texture = { INVALID_HANDLE };
	}
}

bool    ImGui_ImplVulkan_CreateDeviceObjects() {
	VkDevice device = rhi.device;
	VkPhysicalDevice physical_device = rhi.device;
	
	//LOAD SHADERS
	string_buffer vertex_shader, fragment_shader;
	
	if (!io_readf("shaders/cache/imgui_vert.spirv", &vertex_shader)) abort();
	if (!io_readf("shaders/cache/imgui_frag.spirv", &fragment_shader)) abort();

	backend.vert_shader = make_ShaderModule(vertex_shader);
	backend.frag_shader = make_ShaderModule(fragment_shader);

    SamplerDesc sampler = {};
    sampler.min_filter = Filter::Linear;
    sampler.mag_filter = Filter::Linear;
    
	backend.sampler = query_Sampler(sampler);
	backend.dummy_tex = default_textures.white; //todo better ways of doing this

	//CREATE VERTEX BUFFER
	VertexAttrib attribs[3] = {
	{ 2, VertexAttrib::Float, offsetof(ImDrawVert, pos) },
	{ 2, VertexAttrib::Float, offsetof(ImDrawVert, uv) },
	{ 4, VertexAttrib::Unorm, offsetof(ImDrawVert, col) },
	};

	LayoutVertexInputs inputs;
	fill_layout(inputs, VK_VERTEX_INPUT_RATE_VERTEX, { attribs, 3 }, sizeof(ImDrawVert), 0);

	u64 vertex_offset = 0;
	u64 index_offset = 0;

	for (uint i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		backend.vertex_arena[i].base_offset = vertex_offset;
		backend.vertex_arena[i].capacity = MAX_IMGUI_VERTEX_BUFFER_SIZE;

		backend.index_arena[i].base_offset = index_offset;
		backend.index_arena[i].capacity = MAX_IMGUI_INDEX_BUFFER_SIZE;
	
		vertex_offset += MAX_IMGUI_VERTEX_BUFFER_SIZE;
		index_offset += MAX_IMGUI_INDEX_BUFFER_SIZE;
	}

	backend.vertex_buffer = make_HostVisibleBuffer(device, physical_device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertex_offset);
	backend.index_buffer = make_HostVisibleBuffer(device, physical_device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, index_offset);
	
	map_buffer_memory(device, backend.vertex_buffer);
	map_buffer_memory(device, backend.index_buffer);

	//CREATE UBO
	backend.proj_ubo_buffer = alloc_ubo_buffer(sizeof(ImGui_Proj), UBO_PERMANENT_MAP);

	//CREATE DESCRIPTORS
	CombinedSampler sampler_array[MAX_IMGUI_TEXTURES] = {};
	for (uint i = 0; i < MAX_IMGUI_TEXTURES; i++) {
		sampler_array[i].sampler = backend.sampler;
		sampler_array[i].texture = backend.dummy_tex;
	}

	DescriptorDesc descriptor_desc;
	add_ubo(descriptor_desc, VERTEX_STAGE, backend.proj_ubo_buffer, 0);
	add_combined_sampler(descriptor_desc, FRAGMENT_STAGE, { sampler_array, MAX_IMGUI_TEXTURES }, 1);

	for (uint i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		//todo this generates 3 layouts when 1 would suffice
		update_descriptor_set(backend.descriptor[i], descriptor_desc);
	}

	VkDescriptorSetLayout descriptor_layout = get_descriptor_set_layout(backend.descriptor[0]);

	//CREATE PIPELINE
	VkPipelineDesc pipeline_desc = {};
	pipeline_desc.cull_mode = VK_CULL_MODE_NONE;
	pipeline_desc.depth_test_enable = VK_FALSE;
	pipeline_desc.depth_compare_op = VK_COMPARE_OP_ALWAYS;
	pipeline_desc.polygon_mode = VK_POLYGON_MODE_FILL;
	pipeline_desc.alpha_blend_enable = VK_TRUE;
	pipeline_desc.src_blend_factor = VK_BLEND_FACTOR_ONE;
	pipeline_desc.dst_blend_factor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	pipeline_desc.blend_op = VK_BLEND_OP_ADD;

	pipeline_desc.attribute_descriptions = inputs.input_desc;
	pipeline_desc.binding_descriptions = inputs.binding_desc;

	pipeline_desc.vert_shader = backend.vert_shader;
	pipeline_desc.frag_shader = backend.frag_shader;
	pipeline_desc.render_pass = (VkRenderPass)render_pass_by_id(RenderPass::Screen).id;

	VkPushConstantRange push_constant_range = {};
	push_constant_range.offset = 0;
	push_constant_range.size = sizeof(int);
	push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	pipeline_desc.push_constant_range = push_constant_range;
		
	pipeline_desc.descriptor_layouts = descriptor_layout;

	make_GraphicsPipeline(device, pipeline_desc, &backend.pipeline_layout, &backend.pipeline);

	ImGui_ImplVulkan_CreateFontsTexture();

	return true;
}

void    ImGui_ImplVulkan_DestroyDeviceObjects()
{ /*
	if (g_VboHandle) glDeleteBuffers(1, &g_VboHandle);
	if (g_ElementsHandle) glDeleteBuffers(1, &g_ElementsHandle);
	g_VboHandle = g_ElementsHandle = 0;

	if (g_ShaderHandle && g_VertHandle) glDetachShader(g_ShaderHandle, g_VertHandle);
	if (g_VertHandle) glDeleteShader(g_VertHandle);
	g_VertHandle = 0;

	if (g_ShaderHandle && g_FragHandle) glDetachShader(g_ShaderHandle, g_FragHandle);
	if (g_FragHandle) glDeleteShader(g_FragHandle);
	g_FragHandle = 0;

	if (g_ShaderHandle) glDeleteProgram(g_ShaderHandle);
	g_ShaderHandle = 0;

	ImGui_ImplOpenGL3_DestroyFontsTexture();
	*/
}

//--------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the back-end to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
//--------------------------------------------------------------------------------------------------------

/*
static void ImGui_ImplVulkan_RenderWindow(ImGuiViewport* viewport, void*)
{
	//if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear))
	//{
	//	ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	//	glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
	//	glClear(GL_COLOR_BUFFER_BIT);
	//}
	//ImGui_ImplVulkan_RenderDrawData(cmd_buffer, viewport->DrawData);
}

static void ImGui_ImplVulkan_InitPlatformInterface()
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
	platform_io.Renderer_RenderWindow = ImGui_ImplVulkan_RenderWindow;
}
*/

static void ImGui_ImplVulkan_ShutdownPlatformInterface()
{
	//ImGui::DestroyPlatformWindows();
}


