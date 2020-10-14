#pragma once
#include "core/container/string_buffer.h"
#include "core/container/string_view.h"
#include "core/core.h"

//THE ENTIRE IDEA BEHIND THE VFS SYSTEM, WAS TO IMPLEMENT IT IN SUCH A WAY THAT ASSET FILES COULD BE LOOKED UP IN A BINARY
//FILE WHEN PACKING THE GAME. HOWEVER THE HANDLE SYSTEM COULD POTENTIALLY REPLACE THIS COMPLETEY
//MOREOVER IT SEEMS MANY ASSETS COULD BE REUSED BETWEEN LEVELS, SO DOES IT EVEN MAKE SENSE FOR ASSET LOADING TO DIFFERENTIATE THE FOLDER
//THE CURRENT FUNCTIONALITY ALLOWS REDIRECTING THE ASSET PREFIX TO AN ABITRARY FOLDER

struct FS {
	string_buffer base_path;
};

void make_FS();
void destroy_FS();

CORE_API i64  io_time_modified(string_view path);
CORE_API bool io_readf(string_view path, string_buffer* output);
CORE_API bool io_readfb(string_view path, string_buffer* output);
CORE_API bool io_writef(string_view path, string_view contents);
