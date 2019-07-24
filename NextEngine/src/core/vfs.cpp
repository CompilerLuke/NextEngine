#include "stdafx.h"
#include "core/vfs.h"
#include <fstream>
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>

StringBuffer Level::asset_folder_path;

void Level::set_level(StringView asset_folder_path) {
	Level::asset_folder_path = asset_folder_path;
}

StringBuffer Level::asset_path(StringView filename) {
	StringBuffer buffer;
	buffer += asset_folder_path;
	buffer += filename;
	return buffer;
}

bool Level::to_asset_path(StringView filename, StringBuffer* result) {
	if (filename.starts_with(asset_folder_path)) {
		*result = filename.sub(asset_folder_path.size(), filename.size());
		return true;
	}

	return false;
}

File::File() {}

bool File::open(StringView filename, FileMode mode) {
	this->filename = Level::asset_path(filename);

	const char* c_mode = NULL;
	if (mode == FileMode::ReadFile) c_mode = "r";
	if (mode == FileMode::WriteFile) c_mode = "w";
	if (mode == FileMode::ReadFileB) c_mode = "rb";
	if (mode == FileMode::WriteFileB) c_mode = "wb";

	errno_t errors = fopen_s(&this->f, this->filename.c_str(), c_mode);
	return this->f != NULL && !errors;
}

File::~File() {
	if (this->f) fclose(this->f);
}

StringBuffer File::read() {
	struct _stat info[1];

	_stat(filename.c_str(), info);
	size_t length = info->st_size;

	StringBuffer buffer;
	buffer.reserve(length);
	length = fread_s(buffer.data, length, sizeof(char), length, f);
	buffer.data[length] = '\0';
	buffer.length = length;
	return buffer;
}

unsigned int File::read_binary(char** buffer) {
	struct _stat info[1];

	_stat(filename.c_str(), info);
	size_t length = info->st_size;

	*buffer = new char[length + 1]; //todo avoid copy
	return fread_s(*buffer, length, sizeof(char), length, f);
}

void File::write(StringView content) {
	fwrite(content.data, sizeof(char), sizeof(char) * content.size(), f);
}

long long Level::time_modified(StringView filename) {
	auto f = asset_path(filename);
	
	struct _stat buffer;
	if (_stat(f.c_str(), &buffer) != 0) {
		throw StringBuffer("Could not read file ") + f;
	}
	return buffer.st_mtime;
}