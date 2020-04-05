#include "stdafx.h"
#include "graphics/rhi/frame_buffer.h"
#include <glad/glad.h>
#include "graphics/assets/texture.h"

#ifdef RENDER_API_VULKAN

AttachmentDesc::AttachmentDesc(texture_handle& id) 
: tex_id(id) {}

void add_attachment(TextureManager& manager, uint width, uint height, AttachmentDesc& self, GLenum gl_attach) {
	uint tex = gl_gen_texture();

	/**/
	self.tex_id = manager.create_from_gl(tex, self);
	/**/
}

void add_depth_attachment(TextureManager& manager, uint width, uint height, AttachmentDesc& self, DepthBufferSettings& depth_buffer) {
	unsigned int tex = gl_gen_texture();

	/**/
	self.tex_id = manager.create_from_gl(tex, self);

	/**/
}

Framebuffer::Framebuffer(TextureManager& manager, FramebufferDesc& settings) {
	unsigned int fbo = 0;
	unsigned int rbo = 0;

	/**/

	if (settings.depth_buffer == DepthComponent24) {
		if (settings.stencil_buffer == StencilComponent8) {
			/**/
		}
		else {
			/**/
		}
		/**/
	}

	for (int i = 0; i < settings.color_attachments.length; i++) {
		auto& attach = settings.color_attachments[i];
		add_attachment(manager, settings.width, settings.height, attach, GL_COLOR_ATTACHMENT0 + i);
	}

	if (settings.depth_attachment) {
		add_depth_attachment(manager, settings.width, settings.height, *settings.depth_attachment, settings.depth_buffer);
	}

	/**/

	this->fbo = fbo;
	this->rbo = rbo;
	this->width = settings.width;
	this->height = settings.height;
}

void Framebuffer::operator=(Framebuffer&& other) noexcept{
	this->fbo = other.fbo;
	this->rbo = other.rbo;
	this->width = other.width;
	this->height = other.height;

	other.fbo = 0;
	other.rbo = 0;
}

Framebuffer::Framebuffer() {};


void Framebuffer::read_pixels(int x, int y, int width, int height, int internal_format, int format, void* ptr) {
	/**/
}

void Framebuffer::bind() {
	/**/
}

void Framebuffer::clear_color(glm::vec4 color) {
	/**/
}

void Framebuffer::clear_depth(glm::vec4 color) {
	/**/
}

void Framebuffer::unbind() {
	/**/
}

#endif