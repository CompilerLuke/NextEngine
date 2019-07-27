#include <editor/editor.h>
#include <graphics/window.h>
#include <core/vfs.h>
#include <logger/logger.h>
#include <core/string_buffer.h>
#include <iostream>

int main() {
	Window window;
	window.title = "The Unplugging";
	//window.full_screen = true;
	window.init();

	StringView level = "C:\\Users\\User\\Desktop\\TopCCompiler\\TopCompiler\\Fernix\\assets\\level2\\";
	Level::set_level(level);

	//Editor editor("C:\\Users\\User\\source\\repos\\NextEngine\\x64\\Debug\\TheUnpluggingGame.dll", window);

	Editor editor("C:\\Users\\User\\source\\repos\\NextEngine\\x64\\Release\\TheUnpluggingGame.dll", window);

	editor.run();

	return 0;
}