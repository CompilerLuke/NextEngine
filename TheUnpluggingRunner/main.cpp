
#define PROFILING

#include <editor/editor.h>
#include <graphics/window.h>
#include <core/vfs.h>
#include <logger/logger.h>
#include <core/string_buffer.h>
#include <iostream>

#include <core/profiling.h>
#include <stdio.h>

int main() {
	/*
	int num_iterations = 10000;
	const char* world = "World!";

	PROFILE_BEGIN()
		for (int i = 0; i < num_iterations; i++) {
			printf("Hello %s\n", world);
		}
	PROFILE_END(Printf);

	PROFILE_BEGIN()
		for (int i = 0; i < num_iterations; i++) {
			std::cout << "Hello " << world << "\n";
		}
	PROFILE_END(StdoutNewline)

	PROFILE_BEGIN()
		for (int i = 0; i < num_iterations; i++) {
			std::cout << "Hello " << world << std::endl;
		}
	PROFILE_END(StdoutEndline)

	PROFILE_BEGIN()
		for (int i = 0; i < num_iterations; i++) {
			log("Hello ", world);
		}
	PROFILE_END(MyLogger)

	PROFILE_LOG(MyLogger)
	PROFILE_LOG(StdoutEndline)	
	PROFILE_LOG(StdoutNewline)
	PROFILE_LOG(Printf)

	flush_logger();

	while (1) {

	}

	return 0;
	*/

	Window window;
	window.title = "The Unplugging";
	window.full_screen = false;
	//window.width = 1980;
	//window.height = 1080;
	
	window.vSync = true;
	window.init();



	//StringView level = "C:\\Users\\User\\Desktop\\TopCCompiler\\TopCompiler\\Fernix\\assets\\level2\\";
	StringView level = "assets\\level2\\";
	Level::set_level(level);

	//Editor editor("C:\\Users\\User\\source\\repos\\NextEngine\\x64\\Debug\\TheUnpluggingGame.dll", window);

	//Editor editor("C:\\Users\\User\\source\\repos\\NextEngine\\x64\\Release\\TheUnpluggingGame.dll", window);
	Editor editor("TheUnpluggingGame.dll", window);

	editor.run();

	return 0;
}