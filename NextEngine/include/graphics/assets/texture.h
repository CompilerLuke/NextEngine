#pragma once

#include "core/handle.h"
#include "core/container/string_buffer.h"
#include "core/container/handle_manager.h"
#include "core/reflection.h"

struct Level;

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

	REFLECT(NO_ARG)
};

struct ENGINE_API Cubemap {
	string_buffer filename;
	TextureID texture_id = 0;
	void bind_to(unsigned int);

	REFLECT(NO_ARG)
};

struct ENGINE_API CubemapManager : HandleManager<Cubemap, cubemap_handle> {};

struct ENGINE_API TextureManager : HandleManager<Texture, texture_handle> {
	Level& level;
	
	TextureManager(Level& level);
	Image load_Image(string_view filename);
	texture_handle load(string_view filename);
	texture_handle create_from(const Image& image, const TextureDesc& desc = {});
	texture_handle create_from_gl(uint tex_id, const TextureDesc& self);
	texture_handle assign_handle(Texture&&);
	void assign_handle(texture_handle, Texture&&);
	void on_load(texture_handle handle);
};

void ENGINE_API gl_bind_cubemap(CubemapManager&, cubemap_handle, uint);
void ENGINE_API gl_bind_to(TextureManager&, texture_handle, uint);
int  ENGINE_API gl_id_of(TextureManager&, texture_handle);
int  ENGINE_API gl_format(InternalColorFormat format);
int  ENGINE_API gl_format(ColorFormat format);
int  ENGINE_API gl_format(TexelType format);
int  ENGINE_API gl_format(Filter filter);
int  ENGINE_API gl_format(Wrap wrap);
int  ENGINE_API gl_gen_texture();
void ENGINE_API gl_copy_sub(int width, int height); //todo abstract away
uint ENGINE_API gl_submit(Image&);