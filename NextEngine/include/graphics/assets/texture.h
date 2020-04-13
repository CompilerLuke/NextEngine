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
};

enum class InternalColorFormat { Rgb16f, R32I, Red };
enum class ColorFormat { Rgb, Red_Int, Red };
enum class TexelType { Int, Float };
enum class Filter { Nearest, Linear, LinearMipmapLinear };
enum class Wrap { ClampToBorder, Repeat };

struct TextureDesc {
	InternalColorFormat internal_format = InternalColorFormat::Rgb16f;
	ColorFormat external_format = ColorFormat::Rgb;
	TexelType texel_type = TexelType::Float;
	Filter min_filter = Filter::Nearest;
	Filter mag_filter = Filter::Nearest;
	Wrap wrap_s = Wrap::ClampToBorder;
	Wrap wrap_t = Wrap::ClampToBorder;
};

struct Texture {
	string_buffer filename;
	TextureID texture_id = 0;

	REFLECT(ENGINE_API)
};

struct Cubemap {
	string_buffer filename;
	TextureID texture_id = 0;
	void bind_to(unsigned int);

	REFLECT(ENGINE_API)
};

struct Assets;

ENGINE_API Image load_Image(Assets&, string_view, Allocator*);
ENGINE_API void upload_Texture(Assets&, const Image& image, const TextureDesc& desc = {});
ENGINE_API u64 underlying_texture(Assets&, texture_handle handle);