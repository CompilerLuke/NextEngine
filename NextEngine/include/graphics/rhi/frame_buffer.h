#pragma once

#include "core/container/tvector.h"
#include "graphics/assets/texture.h"
#include "graphics/rhi/shader_access.h"
#include "graphics/pass/pass.h"
#include <glm/vec4.hpp>

struct Assets;

enum DepthBufferFormat { Disable_Depth_Buffer, DepthComponent24 };
enum StencilBufferFormat { Disable_Stencil_Buffer, StencilComponent8 };

struct AttachmentDesc {
	int width, height;
	texture_handle* tex_id;
	TextureUsage usage = TextureUsage::Sampled | TextureUsage::InputAttachment | TextureUsage::ColorAttachment;
	TextureLayout initial_layout = TextureLayout::Undefined;
	TextureLayout final_layout = TextureLayout::ShaderReadOptimal;
};

struct Dependency {
	Stage stage;
	RenderPass::ID id;
};

struct FramebufferDesc {
	uint width = 0;
	uint height = 0;
	DepthBufferFormat depth_buffer = DepthComponent24;
	StencilBufferFormat stencil_buffer = Disable_Stencil_Buffer;
	AttachmentDesc* depth_attachment = NULL;
	tvector<AttachmentDesc> color_attachments;
	tvector<Dependency> dependency;
};

struct Framebuffer {
	uint id;
	uint width = 0;
	uint height = 0;

	void ENGINE_API bind();
	void ENGINE_API read_pixels(int x, int y, int width, int height, int internal_format, int format, void* ptr);
	void ENGINE_API clear_color(glm::vec4);
	void ENGINE_API clear_depth(glm::vec4);
	void ENGINE_API unbind();
};

ENGINE_API void add_dependency(FramebufferDesc& desc, Stage, RenderPass::ID);
ENGINE_API AttachmentDesc& add_color_attachment(FramebufferDesc& desc, texture_handle* tex = NULL);
ENGINE_API AttachmentDesc& add_depth_attachment(FramebufferDesc& desc, texture_handle*, DepthBufferFormat format = DepthComponent24);
ENGINE_API void make_Framebuffer(RenderPass::ID, FramebufferDesc&);
ENGINE_API void make_wsi_pass(slice<Dependency>);
ENGINE_API void build_framegraph();

/*
struct Framebuffer {
	uint fbo = 0;
	uint rbo = 0;
	uint width = 0;
	uint height = 0;

	void ENGINE_API operator=(Framebuffer&&) noexcept;
	ENGINE_API Framebuffer(FramebufferDesc&);
	ENGINE_API Framebuffer();

	void ENGINE_API bind();
	void ENGINE_API read_pixels(int x, int y, int width, int height, int internal_format, int format, void* ptr);
	void ENGINE_API clear_color(glm::vec4);
	void ENGINE_API clear_depth(glm::vec4);
	void ENGINE_API unbind();
};
*/