#include "stdafx.h"
#include "core/vfs.h"
#include <fstream>
#include <time.h>
#include <sys/stat.h>
#include <sstream>

std::string Level::asset_folder_path;

void Level::set_level(const std::string& asset_folder_path) {
	Level::asset_folder_path = asset_folder_path;
}

std::string Level::asset_path(const std::string& filename) {
	return asset_folder_path + filename;
}

bool Level::to_asset_path(const std::string& filename, std::string* result) {
	if (filename.find(asset_folder_path) == 0) {
		*result = filename.substr(asset_folder_path.size(), filename.size());
		return true;
	}

	return false;
}

File::File() {}

bool File::open(const std::string& filename) {
	auto full_path = Level::asset_path(filename);

	fstream.open(full_path);
	return !fstream.fail();
}

std::string File::read() {
	std::stringstream strStream;
	strStream << fstream.rdbuf();
	return strStream.str();
}

long long Level::time_modified(const std::string& filename) {
	std::string f = asset_path(filename);
	
	struct _stat buffer;
	if (_stat(f.c_str(), &buffer) != 0) {
		throw std::string("Could not read file ") + f;
	}
	return buffer.st_mtime;
}