#pragma once
#include "core/string_buffer.h"
#include "core/string_view.h"
#include "core/core.h"

namespace Level {
	extern ENGINE_API StringBuffer asset_folder_path;

	StringBuffer ENGINE_API asset_path(StringView filename);
	bool ENGINE_API to_asset_path(StringView filename, StringBuffer* result);
	long long ENGINE_API time_modified(StringView filename);

	void ENGINE_API set_level(StringView filename);
};

struct ENGINE_API File {
	enum FileMode { WriteFile, ReadFile, ReadFileB, WriteFileB };
	FILE* f;
	StringBuffer filename;

	void close();

	File();
	~File();
	bool  open(StringView, FileMode);
	StringBuffer read();
	void write(StringView);
	unsigned int read_binary(char**);
};

void WatchFileChange(StringView, std::function<void>());