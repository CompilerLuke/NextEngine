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

void Texture::on_load() {
	auto real_filename = Level::asset_path(filename);

	stbi_set_flip_vertically_on_load(true);

	int width = 0;
	int height = 0;
	int nr_channels = 0;
	unsigned int texture_id = 0;

	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16);

	auto data = stbi_load(real_filename.c_str(), &width, &height, &nr_channels, 4); //todo might waste space
	if (!data) throw "Could not load texture";

	auto internal_color_format = GL_RGBA;

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, internal_color_format, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);

	stbi_image_free(data);

	this->texture_id = texture_id;
}

Handle<Texture> make_Texture(Texture&& tex) {
	return RHI::texture_manager.make(std::move(tex));
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

Handle<Texture> load_Texture(const std::string& filename) {
	for (int i = 0; i < RHI::texture_manager.slots.length; i++) { //todo move out into Texture manager
		auto& slot = RHI::texture_manager.slots[i];
		if (slot.generation != INVALID_HANDLE && slot.obj.filename == filename) {
			return RHI::texture_manager.index_to_handle(i);
		}
	}

	Texture texture;
	texture.filename = filename;
	texture.on_load();
	return RHI::texture_manager.make(std::move(texture));
}

