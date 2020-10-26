#pragma once

#include "core/container/tvector.h"
#include "graphics/assets/texture.h"
#include "graphics/rhi/shader_access.h"
#include "graphics/pass/pass.h"
#include <glm/vec4.hpp>

struct Assets;

enum class DepthBufferFormat { None, P24 };
enum class StencilBufferFormat { None, P8 };
enum class LoadOp {Load, Clear, DontCare};
enum class StoreOp {Store, DontCare};

struct AttachmentDesc {
	texture_handle* tex_id;
	TextureUsage usage = TextureUsage::Sampled | TextureUsage::InputAttachment | TextureUsage::ColorAttachment;
	TextureLayout initial_layout = TextureLayout::Undefined;
	TextureLayout final_layout = TextureLayout::ShaderReadOptimal;
	TextureFormat format = TextureFormat::UNORM;
	LoadOp load_op = LoadOp::Clear;
	StoreOp store_op = StoreOp::Store;
	uint num_channels = 4;
	uint num_mips = 1;
};

struct Dependency {
	Stage stage;
	RenderPass::ID id;
};

struct FramebufferDesc {
	uint width = 0;
	uint height = 0;
	DepthBufferFormat depth_buffer = DepthBufferFormat::P24;
	StencilBufferFormat stencil_buffer = StencilBufferFormat::None;
	AttachmentDesc* depth_attachment = NULL;
	tvector<AttachmentDesc> color_attachments;
	tvector<Dependency> dependency;
};

struct SubpassDesc {
	tvector<uint> color_attachments;
	bool depth_attachment;
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
ENGINE_API AttachmentDesc& add_depth_attachment(FramebufferDesc& desc, texture_handle*, DepthBufferFormat format = DepthBufferFormat::P24);
ENGINE_API void make_Framebuffer(RenderPass::ID, FramebufferDesc&);
ENGINE_API void make_Framebuffer(RenderPass::ID, FramebufferDesc&, slice<SubpassDesc>);
ENGINE_API void make_wsi_pass(slice<Dependency>);
ENGINE_API void build_framegraph();
ENGINE_API void submit_framegraph(); //ONLY USED FOR LOADING

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