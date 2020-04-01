#pragma once

#include "graphics/assets/texture.h"
#include <glm/vec4.hpp>

enum DepthBufferSettings { DepthComponent24 };
enum StencilBufferSettings { Disable_Stencil_Buffer, StencilComponent8 };

struct ENGINE_API AttachmentDesc : TextureDesc {
	texture_handle& tex_id;
	AttachmentDesc(texture_handle&);
};

struct ENGINE_API FramebufferDesc {
	uint width = 0;
	uint height = 0;
	DepthBufferSettings depth_buffer = DepthComponent24;
	StencilBufferSettings stencil_buffer = Disable_Stencil_Buffer;
	AttachmentDesc* depth_attachment = NULL;
	vector<AttachmentDesc> color_attachments;
};

struct ENGINE_API Framebuffer {
	uint fbo = 0;
	uint rbo = 0;
	uint width = 0;
	uint height = 0;

	void operator=(Framebuffer&&) noexcept;
	Framebuffer(TextureManager&, FramebufferDesc&);
	Framebuffer();

	void bind();
	void read_pixels(int x, int y, int width, int height, int internal_format, int format, void* ptr);
	void clear_color(glm::vec4);
	void clear_depth(glm::vec4);
	void unbind();
};