#pragma once
#include "core/string_buffer.h"
#include "core/string_view.h"
#include "core/core.h"

namespace Level {
	extern StringBuffer asset_folder_path;

	StringBuffer ENGINE_API asset_path(StringView filename);
	bool ENGINE_API to_asset_path(StringView filename, StringBuffer* result);
	long long ENGINE_API time_modified(StringView filename);

	void ENGINE_API set_level(StringView filename);
};

struct File {
	enum FileMode { WriteFile, ReadFile, ReadFileB, WriteFileB };
	FILE* f;
	StringBuffer filename;

	ENGINE_API File();
	ENGINE_API ~File();
	bool ENGINE_API open(StringView, FileMode);
	ENGINE_API StringBuffer read();
	ENGINE_API void write(StringView);
	ENGINE_API unsigned int read_binary(char**);
};