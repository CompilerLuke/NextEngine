#pragma once

#include "graphics/assets/texture.h"
#include <glm/vec4.hpp>

struct Assets;

enum DepthBufferSettings { DepthComponent24 };
enum StencilBufferSettings { Disable_Stencil_Buffer, StencilComponent8 };

struct AttachmentDesc : TextureDesc {
	texture_handle& tex_id;
	ENGINE_API AttachmentDesc(texture_handle&);
};

struct FramebufferDesc {
	uint width = 0;
	uint height = 0;
	DepthBufferSettings depth_buffer = DepthComponent24;
	StencilBufferSettings stencil_buffer = Disable_Stencil_Buffer;
	AttachmentDesc* depth_attachment = NULL;
	vector<AttachmentDesc> color_attachments;
};

struct Framebuffer {
	uint fbo = 0;
	uint rbo = 0;
	uint width = 0;
	uint height = 0;

	void ENGINE_API operator=(Framebuffer&&) noexcept;
	ENGINE_API Framebuffer(Assets&, FramebufferDesc&);
	ENGINE_API Framebuffer();

	void ENGINE_API bind();
	void ENGINE_API read_pixels(int x, int y, int width, int height, int internal_format, int format, void* ptr);
	void ENGINE_API clear_color(glm::vec4);
	void ENGINE_API clear_depth(glm::vec4);
	void ENGINE_API unbind();
};