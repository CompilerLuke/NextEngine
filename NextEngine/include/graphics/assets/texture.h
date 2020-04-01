#pragma once

#include "core/handle.h"
#include "core/container/string_buffer.h"
#include "core/container/handle_manager.h"
#include "core/reflection.h"

using TextureID = uint;

struct ENGINE_API Image {
	int width = 0;
	int height = 0;
	int num_channels = 0;
	void* data = NULL;

	Image();
	Image(Image&&);
	~Image();
};

enum class InternalColorFormat { Rgb16f, R32I, Red };
enum class ColorFormat { Rgb, Red_Int, Red };
enum class TexelType { Int, Float };
enum class Filter { Nearest, Linear, LinearMipmapLinear };
enum class Wrap { ClampToBorder, Repeat };

struct ENGINE_API TextureDesc {
	InternalColorFormat internal_format = InternalColorFormat::Rgb16f;
	ColorFormat external_format = ColorFormat::Rgb;
	TexelType texel_type = TexelType::Float;
	Filter min_filter = Filter::Nearest;
	Filter mag_filter = Filter::Nearest;
	Wrap wrap_s = Wrap::ClampToBorder;
	Wrap wrap_t = Wrap::ClampToBorder;
};

struct ENGINE_API Texture {
	string_buffer filename;
	TextureID texture_id = 0;

	void submit(Image&);
	void on_load();

	REFLECT(NO_ARG)
};

struct ENGINE_API Cubemap {
	string_buffer filename;
	TextureID texture_id = 0;
	void bind_to(unsigned int);

	REFLECT(NO_ARG)
};

ENGINE_API struct CubemapManager : HandleManager<Cubemap, cubemap_handle> {};

ENGINE_API struct TextureManager : HandleManager<Texture, texture_handle> {
	Image load_Image(string_view filename);
	texture_handle load(string_view filename);
	texture_handle create_from(const Image& image, const TextureDesc& desc = {});
	texture_handle create_from_gl(uint tex_id, const TextureDesc& self);
	texture_handle assign_handle(Texture&&);
};

ENGINE_API void gl_bind_cubemap(CubemapManager&, cubemap_handle, uint);
ENGINE_API void gl_bind_to(TextureManager&, texture_handle, uint);
ENGINE_API int  gl_id_of(TextureManager&, texture_handle);
ENGINE_API int  gl_format(InternalColorFormat format);
ENGINE_API int  gl_format(ColorFormat format);
ENGINE_API int  gl_format(TexelType format);
ENGINE_API int  gl_format(Filter filter);
ENGINE_API int  gl_format(Wrap wrap);
ENGINE_API int  gl_gen_texture();
ENGINE_API void gl_copy_sub(int width, int height); //todo abstract away