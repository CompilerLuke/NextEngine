// ReflectionTool.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <filesystem>
//#include "core/string_view.h"

namespace fs = std::experimental::filesystem;

int main(int argc, const char** args)
{
	std::string path = args[1];
	for (const auto & entry : fs::directory_iterator(path)) {
		//if (StringView(entry.path().c_str()).ends_with(".h")) {

		//}
	}
	//std::cout << StringView(entry.path()) << std::endl;
    return 0;
}

