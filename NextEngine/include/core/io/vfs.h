#pragma once
#include "core/container/string_buffer.h"
#include "core/container/string_view.h"
#include "core/core.h"

//THE ENTIRE IDEA BEHIND THE VFS SYSTEM, WAS TO IMPLEMENT IT IN SUCH A WAY THAT ASSET FILES COULD BE LOOKED UP IN A BINARY
//FILE WHEN PACKING THE GAME. HOWEVER THE HANDLE SYSTEM COULD POTENTIALLY REPLACE THIS COMPLETEY
//MOREOVER IT SEEMS MANY ASSETS COULD BE REUSED BETWEEN LEVELS, SO DOES IT EVEN MAKE SENSE FOR ASSET LOADING TO DIFFERENTIATE
//THE CURRENT FUNCTIONALITY ALLOWS REDIRECTING THE ASSET PREFIX TO AN ABITRARY FOLDER

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
