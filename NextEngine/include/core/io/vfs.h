#pragma once
#include "core/container/string_buffer.h"
#include "core/container/string_view.h"
#include "core/core.h"

//THE ENTIRE IDEA BEHIND THE VFS SYSTEM, WAS TO IMPLEMENT IT IN SUCH A WAY THAT ASSET FILES COULD BE LOOKED UP IN A BINARY
//FILE WHEN PACKING THE GAME. HOWEVER THE HANDLE SYSTEM COULD POTENTIALLY REPLACE THIS COMPLETEY
//MOREOVER IT SEEMS MANY ASSETS COULD BE REUSED BETWEEN LEVELS, SO DOES IT EVEN MAKE SENSE FOR ASSET LOADING TO DIFFERENTIATE THE FOLDER
//THE CURRENT FUNCTIONALITY ALLOWS REDIRECTING THE ASSET PREFIX TO AN ABITRARY FOLDER

struct Assets;

ENGINE_API i64  time_modified(Assets&, string_view path);
ENGINE_API bool readf(Assets&, string_view path, string_buffer* output);
ENGINE_API bool readfb(Assets&, string_view path, string_buffer* output);
ENGINE_API bool writef(Assets&, string_view path, string_view contents);
