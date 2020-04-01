#include "stdafx.h"
#include "graphics/rhi/frame_buffer.h"
#include <glad/glad.h>
#include "graphics/assets/texture.h"

AttachmentDesc::AttachmentDesc(texture_handle& id) 
: tex_id(id) {}

void add_attachment(TextureManager& manager, uint width, uint height, AttachmentDesc& self, GLenum gl_attach) {
	uint tex = gl_gen_texture();

	glTexImage2D(GL_TEXTURE_2D, 0, gl_format(self.internal_format), width, height, 0, gl_format(self.external_format), gl_format(self.texel_type), NULL);
	self.tex_id = manager.create_from_gl(tex, self);
	glFramebufferTexture2D(GL_FRAMEBUFFER, gl_attach, GL_TEXTURE_2D, tex, 0);
}

void add_depth_attachment(TextureManager& manager, uint width, uint height, AttachmentDesc& self, DepthBufferSettings& depth_buffer) {
	unsigned int tex = gl_gen_texture();

	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, gl_format(self.texel_type), NULL);
	self.tex_id = manager.create_from_gl(tex, self);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
}

Framebuffer::Framebuffer(TextureManager& manager, FramebufferDesc& settings) {
	unsigned int fbo;
	unsigned int rbo;

	glGenFramebuffers(1, &fbo);
	glGenRenderbuffers(1, &rbo);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo);

	if (settings.depth_buffer == DepthComponent24) {
		if (settings.stencil_buffer == StencilComponent8) {
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, settings.width, settings.height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fbo);
		}
		else {
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, settings.width, settings.height);
		}
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fbo);
	}

	for (int i = 0; i < settings.color_attachments.length; i++) {
		auto& attach = settings.color_attachments[i];
		add_attachment(manager, settings.width, settings.height, attach, GL_COLOR_ATTACHMENT0 + i);
	}

	if (settings.depth_attachment) {
		add_depth_attachment(manager, settings.width, settings.height, *settings.depth_attachment, settings.depth_buffer);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

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
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glReadPixels(x, y, width, height, internal_format, format, &ptr);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::bind() {
	glViewport(0, 0, width, height);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void Framebuffer::clear_color(glm::vec4 color) {
	glClearColor(color.r, color.g, color.b, color.a);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Framebuffer::clear_depth(glm::vec4 color) {
	glClearColor(color.r, color.g, color.b, color.a);
	glClear(GL_DEPTH_BUFFER_BIT);
}

void Framebuffer::unbind() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}