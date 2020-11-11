#include "assets/dialog.h"
#include "graphics/rhi/window.h"
#include "graphics/assets/assets.h"

#ifdef NE_WINDOWS
#include <Windows.h>
#include <commdlg.h>

string_buffer open_dialog(Window& window) {
	string_view asset_folder_path = current_asset_path_folder();
	char filename[MAX_PATH];

	OPENFILENAMEA ofn;
	memset(&filename, 0, sizeof(filename));
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);

	ofn.hwndOwner = (HWND)window.get_win32_window();
	ofn.lpstrFilter = "All Files\0*.*\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;
	ofn.lpstrDefExt = "";
	ofn.lpstrInitialDir = asset_folder_path.c_str();

	bool success = GetOpenFileNameA(&ofn);

	if (success) {
		return filename;
	}

	return "";
}

#elif __APPLE__

string_buffer open_dialog(Window& window) {
    string_buffer asset_folder_path = current_asset_path_folder();
    
    //TODO COCOA IMPLEMENTATION
    return "";
}

#endif
