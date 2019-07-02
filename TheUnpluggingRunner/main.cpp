#include <editor/editor.h>
#include <graphics/window.h>
#include <core/vfs.h>
#include <logger/logger.h>

int main() {
	Window window;
	window.title = "The Unplugging";
	//window.full_screen = true;
	window.init();

	std::string level = "C:\\Users\\User\\Desktop\\TopCCompiler\\TopCompiler\\Fernix\\assets\\level2\\";
	Level::set_level(level);

	Editor editor("C:\\Users\\User\\source\\repos\\NextEngine\\x64\\Debug\\TheUnpluggingGame.dll", window);
	editor.run();

	return 0;
}