#pragma once

#include <string>
#include "core/handle.h"
#include "reflection/reflection.h"

using TextureID = unsigned int;

struct Texture {
	std::string filename;
	TextureID texture_id = 0;

	void on_load();

	REFLECT()
};

namespace texture {
	void bind_to(Handle<Texture>, unsigned int);
};

struct Cubemap {
	std::string filename;
	TextureID texture_id = 0;
	void bind_to(unsigned int);

	REFLECT()
};

namespace cubemap {
	void bind_to(Handle<Cubemap>, unsigned int);
};


Handle<Texture> load_Texture(const std::string& filename);
Handle<Texture> make_Texture(Texture&&);
Handle<Cubemap> make_Cubemap(Cubemap&&);