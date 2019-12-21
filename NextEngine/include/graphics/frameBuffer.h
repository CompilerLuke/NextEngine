#pragma once

#include "ecs/ecs.h"
#include <vector>
#include <glm/vec4.hpp>

enum DepthBufferSettings { DepthComponent24 };
enum StencilBufferSettings { Disable_Stencil_Buffer, StencilComponent8 };
enum InternalColorFormat { Rgb16f, R32I };
enum ColorFormat { Rgb, Red_Int };
enum TexelType { Int_Texel, Float_Texel };
enum Filter { Nearest, Linear };
enum Wrap { ClampToBorder, Repeat };

struct ENGINE_API AttachmentSettings {
	Handle<struct Texture>& tex_id;
	InternalColorFormat internal_format = Rgb16f;
	ColorFormat external_format = Rgb;
	TexelType texel_type = Float_Texel;
	Filter min_filter = Nearest;
	Filter mag_filter = Nearest;
	Wrap wrap_s = ClampToBorder;
	Wrap wrap_t = ClampToBorder;

	AttachmentSettings(Handle<struct Texture>&);
};

struct ENGINE_API FramebufferSettings {
	unsigned int width = 0;
	unsigned int height = 0;
	DepthBufferSettings depth_buffer = DepthComponent24;
	StencilBufferSettings stencil_buffer = Disable_Stencil_Buffer;
	AttachmentSettings* depth_attachment = NULL;
	vector<AttachmentSettings> color_attachments;
};

struct ENGINE_API Framebuffer {
	unsigned int fbo = 0;
	unsigned int rbo = 0;
	unsigned int width = 0;
	unsigned int height = 0;

	void operator=(Framebuffer&&) noexcept;
	Framebuffer(FramebufferSettings&);
	Framebuffer();

	void bind();
	void clear_color(glm::vec4);
	void clear_depth(glm::vec4);
	void unbind();
};