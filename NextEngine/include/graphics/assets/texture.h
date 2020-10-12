#pragma once

#include "core/handle.h"
#include "core/container/string_buffer.h"
#include "core/container/handle_manager.h"
#include "core/reflection.h"

struct Level;

using TextureID = uint;


//todo these need to be revised


enum class TextureFormat { UNORM, SRGB, HDR, U8 };
enum class Filter { Nearest, Linear };
enum class Wrap { ClampToBorder, Repeat };
enum class TextureLayout { Undefined, ColorAttachmentOptimal, TransferSrcOptimal, TransferDstOptimal, ShaderReadOptimal };
enum class TextureUsage { 
	TransferSrc     = 1 << 0, 
	TransferDst     = 1 << 1, 
	Sampled         = 1 << 2,
	ColorAttachment = 1 << 3,
	InputAttachment = 1 << 4
};

inline TextureUsage operator|(TextureUsage a, TextureUsage b) { return (TextureUsage)((uint)a | (uint)b); }
inline void operator|=(TextureUsage& self, TextureUsage flag) { self = (TextureUsage)((uint)self | (uint)flag); }
inline bool operator&(TextureUsage a, TextureUsage b) {return (uint)a & (uint)b; }

//enum class InternalColorFormat { S8, I32 };
//enum class ColorFormat { Rgb, Rgba, Red_Int, Red };

//todo potentially move into rhi folder
struct TextureDesc {
	TextureFormat format = TextureFormat::UNORM;
	int width, height, num_channels;
	TextureUsage usage = TextureUsage::Sampled | TextureUsage::TransferDst;
	uint num_mips = 1;
};

struct Image : TextureDesc {
	void* data;
};

struct SamplerDesc {
	Filter min_filter = Filter::Nearest;
	Filter mag_filter = Filter::Nearest;
	Filter mip_mode = Filter::Nearest;
	Wrap wrap_u = Wrap::ClampToBorder;
	Wrap wrap_v = Wrap::ClampToBorder;
	uint max_anisotropy = 0;

	//Could write meta program to generate POD equality
	bool operator==(const SamplerDesc& other) const {
		return min_filter == other.min_filter
			&& mag_filter == other.mag_filter
			&& mip_mode == other.mip_mode
			&& wrap_u == other.wrap_u
			&& wrap_v == other.wrap_v
			&& max_anisotropy == other.max_anisotropy;
	}
	
	bool operator!=(const SamplerDesc& other) const {
		return !(*this == other);
	}
};

struct sampler_handle {
	uint id;
};

struct ImageOffset {
	uint x;
	uint y;
};

ENGINE_API void blit_image(struct CommandBuffer& cmd_buffer, Filter filter, texture_handle src, ImageOffset src_region[2], texture_handle dst, ImageOffset dst_region[2]);
ENGINE_API void transition_layout(struct CommandBuffer& cmd_buffer, texture_handle, TextureLayout from, TextureLayout to);

ENGINE_API Image load_Image(string_view, bool reverse=false);
ENGINE_API void free_Image(Image& image);
ENGINE_API texture_handle upload_Texture(const Image& image, bool serialized=false);
ENGINE_API u64 underlying_texture(texture_handle handle);

ENGINE_API sampler_handle query_Sampler(const SamplerDesc&);

ENGINE_API texture_handle alloc_Texture(const TextureDesc&);

//ENGINE_API sampler_handle get_sampler(texture_handle);
//ENGINE_API sampler_handle get_sampler(cubemap_handle);