#pragma once
#include <string>
#include <fstream>
#include "core/core.h"

namespace Level {
	extern std::string asset_folder_path;

	std::string ENGINE_API asset_path(const std::string& filename);
	bool ENGINE_API to_asset_path(const std::string& filename, std::string* result);
	long long ENGINE_API time_modified(const std::string& filename);

	void ENGINE_API set_level(const std::string& filename);
};

struct File {
	std::ifstream fstream;

	ENGINE_API File();
	bool ENGINE_API open(const std::string&);
	ENGINE_API std::string read();
};