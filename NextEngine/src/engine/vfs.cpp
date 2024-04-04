#include "engine/vfs.h"
#include "graphics/assets/assets.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <sstream>

#ifdef __APPLE__
#define _stat stat
#endif

FILE* open(string_view full_filepath, const char* mode) {
	return fopen(full_filepath.c_str(), mode);
}

FILE* open_rel(string_view filepath, const char* mode) {
	return open(tasset_path(filepath).c_str(), mode);
}

bool read_file(string_view filepath, string_buffer* buffer, bool binary) {
    string_buffer full_filepath = tasset_path(filepath);
	FILE* f = open(full_filepath, binary ? "rb" : "r");
	if (!f) return false;

    fseek(f, 0, SEEK_END);
    size_t length = ftell(f);
	buffer->reserve(length);
    fseek(f, 0, SEEK_SET);
	length = fread(buffer->data, sizeof(char), length, f);


	buffer->data[length] = '\0';
	buffer->length = length;

	fclose(f);
	return true;
}

bool io_readf(string_view filepath, string_buffer* buffer) {
	return read_file(filepath, buffer, false);
}

bool io_readfb(string_view filepath, string_buffer* buffer) {
	return read_file(filepath, buffer, true);
}

bool io_writef(string_view filename, string_view content) {
	FILE* f = open_rel(filename, "wb");
	if (!f) return false;
	
	fwrite(content.data, sizeof(char), sizeof(char) * content.size(), f);
	fclose(f);

	return true;
}

i64 io_time_modified(string_view filename) {
	auto f = tasset_path(filename);
    try {
        return std::filesystem::last_write_time(f.c_str()).time_since_epoch().count();
    } catch(std::exception& e) {
        return 0;
    }
}

#ifdef NE_PLATFORM_WINDOWS
const char* SEPERATOR = "\\";
#elif NE_PLATFORM_MACOSX
const char* SEPERATOR = "/";
#endif

bool path_absolute(string_view path, string_buffer* abs) {
    if (!io_get_current_dir(abs)) return false;
    
    bool is_absolute = path.starts_with(*abs);
    if (is_absolute) {
        *abs = path;
    }
    else {
        *abs += SEPERATOR;
        *abs += path;
    }

    return true;
}

bool io_copyf(string_view src, string_view dst, bool fail_if_exists) {
	std::filesystem::copy(src.c_str(), dst.c_str(), fail_if_exists ? std::filesystem::copy_options::none : std::filesystem::copy_options::overwrite_existing);
	return true;
}

bool io_get_current_dir(string_buffer* output) {
	std::filesystem::path p = std::filesystem::current_path();
	*output = p.string().c_str();
    return true;
}