#pragma once
#include "core/container/string_buffer.h"
#include "core/container/string_view.h"
#include "core/core.h"

struct Level {
	string_buffer asset_folder_path;

	string_buffer ENGINE_API asset_path(string_view filename);
	bool ENGINE_API to_asset_path(string_view filename, string_buffer* result);
	long long ENGINE_API time_modified(string_view filename);

	ENGINE_API Level(string_view);
	void ENGINE_API set(string_view filename);
};

struct ENGINE_API File {
	enum FileMode { WriteFile, ReadFile, ReadFileB, WriteFileB };
	Level& level;
	FILE* f;
	string_buffer filename;

	void close();

	File(Level&);
	~File();
	bool  open(string_view, FileMode);
	string_buffer read();
	void write(string_view);
	unsigned int read_binary(char**);
};
