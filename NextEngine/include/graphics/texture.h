#pragma once

#include <string>
#include "core/handle.h"
#include "core/string_buffer.h"
#include "reflection/reflection.h"

using TextureID = unsigned int;

struct ENGINE_API Image {
	int width = 0;
	int height = 0;
	int num_channels = 0;
	void* data = NULL;

	Image();
	Image(Image&&);
	~Image();
};

struct ENGINE_API Texture {
	StringBuffer filename;
	TextureID texture_id = 0;

	void submit(Image&);
	void on_load();

	REFLECT(NO_ARG)
};

namespace texture {
	void ENGINE_API bind_to(Handle<Texture>, unsigned int);
	unsigned int ENGINE_API id_of(Handle<Texture>);
};

struct ENGINE_API Cubemap {
	StringBuffer filename;
	TextureID texture_id = 0;
	void bind_to(unsigned int);

	REFLECT(NO_ARG)
};

namespace cubemap {
	void bind_to(Handle<Cubemap>, unsigned int);
};

Image ENGINE_API load_Image(StringView filename);

Handle<Texture> ENGINE_API load_Texture(StringView filename);
Handle<Texture> ENGINE_API make_Texture(Texture&&);
Handle<Cubemap> ENGINE_API make_Cubemap(Cubemap&&);