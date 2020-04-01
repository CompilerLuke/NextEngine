#include "stdafx.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "graphics/assets/texture.h"
#include "glad/glad.h"
#include "core/io/vfs.h"

REFLECT_STRUCT_BEGIN(Texture)
REFLECT_STRUCT_MEMBER(filename)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Cubemap)
REFLECT_STRUCT_MEMBER(filename)
REFLECT_STRUCT_END()

Image load_Image(string_view filename) {
	string_buffer real_filename = gb::level.asset_path(filename);

	if (filename.starts_with("Aset") || filename.starts_with("tgh") || filename.starts_with("ta") || filename.starts_with("smen")) stbi_set_flip_vertically_on_load(false);
	else stbi_set_flip_vertically_on_load(true);

	Image image;

	image.data = stbi_load(real_filename.c_str(), &image.width, &image.height, &image.num_channels, 4); //todo might waste space
	if (!image.data) throw "Could not load texture";

	return image;
}

Image::Image() {}

Image::Image(Image&& other) {
	this->data = other.data;
	this->width = other.width;
	this->height = other.height;
	this->num_channels = other.num_channels;
	other.data = NULL;
}

Image::~Image() {
	if (data) stbi_image_free(data);
}

void Texture::submit(Image& image) {
	unsigned int texture_id = 0;

	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16);

	auto internal_color_format = GL_RGBA;

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, internal_color_format, GL_UNSIGNED_BYTE, image.data);
	glGenerateMipmap(GL_TEXTURE_2D);

	this->texture_id = texture_id;
}

void Texture::on_load() {
	Image image = load_Image(this->filename);
	submit(image);
}

texture_handle TextureManager::assign_handle(Texture&& tex) {
	bool serialized = tex.filename.length > 0;
	return HandleManager::assign_handle(std::move(tex), serialized);
}

void gl_bind_to(TextureManager& manager, texture_handle handle, unsigned int num) {
	glActiveTexture(GL_TEXTURE0 + num);
	glBindTexture(GL_TEXTURE_2D, gl_id_of(manager, handle));
}

int gl_id_of(TextureManager& manager, texture_handle handle) {
	if (handle.id == INVALID_HANDLE) return 0;
	return manager.get(handle)->texture_id;
}

texture_handle TextureManager::load(string_view filename) {
	for (int i = 0; i < slots.length; i++) { //todo move out into Texture manager
		auto& slot = slots[i];
		if (slot.filename == filename) {
			return index_to_handle(i);
		}
	}

	Texture texture;
	texture.filename = filename;
	texture.on_load();
	return assign_handle(std::move(texture));
}

void gl_copy_sub(int width, int height) {
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
	glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);
}

//todo this should really be an array lookup
int to_opengl(InternalColorFormat format) {
	if (format == InternalColorFormat::Rgb16f) return GL_RGB16F;
	if (format == InternalColorFormat::R32I) return GL_R32I;
	if (format == InternalColorFormat::Red) return GL_RED;
	return 0;
}

int to_opengl(ColorFormat format) {
	if (format == ColorFormat::Rgb) return GL_RGB;
	if (format == ColorFormat::Red_Int) return GL_RED_INTEGER;
	if (format == ColorFormat::Red) return GL_RED;
	return 0;
}

int to_opengl(TexelType format) {
	if (format == TexelType::Float) return GL_FLOAT;
	if (format == TexelType::Int) return GL_INT;
	return 0;
}

int to_opengl(Filter filter) {
	if (filter == Filter::Nearest) return GL_NEAREST;
	if (filter == Filter::Linear) return GL_LINEAR;
	if (filter == Filter::LinearMipmapLinear) return GL_LINEAR_MIPMAP_LINEAR;
	return 0;
}

int to_opengl(Wrap wrap) {
	if (wrap == Wrap::ClampToBorder) return GL_CLAMP_TO_BORDER;
	if (wrap == Wrap::Repeat) return GL_REPEAT;
	return 0;
}

int gen_gl_texture() {
	unsigned int tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	return tex;
}

texture_handle set_texture_settings(TextureManager& texture_manager, const TextureDesc& self, unsigned int tex) {
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, to_opengl(self.min_filter));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, to_opengl(self.mag_filter));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, to_opengl(self.wrap_s));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, to_opengl(self.wrap_t));

	Texture texture;
	texture.texture_id = tex;

	return texture_manager.assign_handle(std::move(texture));
}

texture_handle TextureManager::create_from(const Image& image, const TextureDesc& self) {
	int tex = gl_gen_texture();
	texture_handle handle = set_texture_settings(*this, self, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, to_opengl(self.internal_format), image.width, image.height, 0, to_opengl(self.external_format), to_opengl(self.texel_type), image.data);
		
	return handle;
}

void gl_bind_to(CubemapManager& cubemap_manager, cubemap_handle handle, unsigned int num) {
	Cubemap* tex = cubemap_manager.get(handle);
	glActiveTexture(GL_TEXTURE0 + num);
	glBindTexture(GL_TEXTURE_CUBE_MAP, tex->texture_id);
}

