#include "stdafx.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "graphics/texture.h"
#include "ecs/ecs.h"
#include "graphics/rhi.h"
#include "glad/glad.h"
#include "core/vfs.h"

REFLECT_GENERIC_STRUCT_BEGIN(Handle<Texture>)
REFLECT_STRUCT_MEMBER(id)
REFLECT_STRUCT_END()

REFLECT_GENERIC_STRUCT_BEGIN(Handle<Cubemap>)
REFLECT_STRUCT_MEMBER(id)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Texture)
REFLECT_STRUCT_MEMBER(filename)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Cubemap)
REFLECT_STRUCT_MEMBER(filename)
REFLECT_STRUCT_END()

Image load_Image(StringView filename) {
	StringBuffer real_filename = Level::asset_path(filename);

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

Handle<Texture> make_Texture(Texture&& tex) {
	bool serialized = tex.filename.length > 0;
	return RHI::texture_manager.make(std::move(tex), serialized);
}

Handle<Cubemap> make_Cubemap(Cubemap&& tex) {
	return RHI::cubemap_manager.make(std::move(tex));
}

namespace texture {
	void bind_to(Handle<Texture> handle, unsigned int num) {
		glActiveTexture(GL_TEXTURE0 + num);
		glBindTexture(GL_TEXTURE_2D, id_of(handle));
	}

	unsigned int id_of(Handle<Texture> handle) {
		if (handle.id == INVALID_HANDLE) return 0;
		return RHI::texture_manager.get(handle)->texture_id;
	}
}

namespace cubemap {
	void bind_to(Handle<Cubemap> handle, unsigned int num) {
		Cubemap* tex = RHI::cubemap_manager.get(handle);
		glActiveTexture(GL_TEXTURE0 + num);
		glBindTexture(GL_TEXTURE_CUBE_MAP, tex->texture_id);
	}
}

Handle<Texture> load_Texture(StringView filename) {
	for (int i = 0; i < RHI::texture_manager.slots.length; i++) { //todo move out into Texture manager
		auto& slot = RHI::texture_manager.slots[i];
		if (slot.filename == filename) {
			return RHI::texture_manager.index_to_handle(i);
		}
	}

	Texture texture;
	texture.filename = filename;
	texture.on_load();
	return make_Texture(std::move(texture));
}
