#include "engine/vfs.h"
#include "graphics/assets/assets.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>

#ifdef __APPLE__
#define _stat stat
#endif

FILE* open(string_view full_filepath, const char* mode) {
	return fopen(full_filepath.c_str(), mode);
}

FILE* open_rel(string_view filepath, const char* mode) {
	return open(tasset_path(filepath).c_str(), mode);
}

bool read_file(string_view filepath, string_buffer* buffer, int null_terminated) {
	string_buffer full_filepath = tasset_path(filepath);
	FILE* f = open(full_filepath, "rb");
	if (!f) return false;

	struct _stat info[1];

	_stat(full_filepath.c_str(), info);
	size_t length = info->st_size;

	buffer->reserve(length + null_terminated);
	length = fread(buffer->data, sizeof(char), length, f);
    
	if (null_terminated) buffer->data[length] = '\0';
	buffer->length = length;

	fclose(f);

	return true;
}

bool io_readf(string_view filepath, string_buffer* buffer) {
	return read_file(filepath, buffer, 1);
}

bool io_readfb(string_view filepath, string_buffer* buffer) {
	return read_file(filepath, buffer, 0);
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
	
	struct _stat buffer;
	if (_stat(f.c_str(), &buffer) != 0) { return -1; };

	return buffer.st_mtime;
}

#ifdef NE_PLATFORM_WINDOWS
#include <Windows.h>

bool io_copyf(string_view src, string_view dst, bool fail_if_exists) {
	return CopyFileA(src.c_str(), dst.c_str(), true);
}

bool io_get_current_dir(string_buffer* output) {
	char buffer[100];
	uint len = GetCurrentDirectoryA(100, buffer);
	*output = buffer;
	return len > 0;
}

bool path_absolute(string_view path, string_buffer* abs) {
	if (!io_get_current_dir(abs)) return false;
	
	bool is_absolute = path.starts_with(*abs);
	if (is_absolute) {
		*abs = path;
	}
	else {
		*abs += "\\";
		*abs += path;
	}

	return true;
}

#elif NE_PLATFORM_MACOSX
#include <copyfile.h>

bool io_copyf(string_view src, string_view dst, bool fail_if_exists) {
    copyfile_state_t state = copyfile_state_alloc();
    
    bool result = copyfile(src.c_str(), dst.c_str(), state, fail_if_exists ? COPYFILE_ALL | COPYFILE_EXCL : COPYFILE_ALL);
    copyfile_state_free(state);
    
    return result;
}
#endif

wchar_t* to_wide_char(const char* orig);

/*
void WatchFileChange(string_view file, std::function<void()> func) {
	static int iCount = 0;
	DWORD dwWaitStatus;
	HANDLE dwChangeHandles;

	const char* ptcFileBaseDir = file.sub(0, file.find_last_of('/')).c_str();
	const char* ptcFileName = file.sub(file.find_last_of('/') + 1, file.length).c_str();

	if (!ptcFileBaseDir || !ptcFileName) return;

	std::wstring wszFileNameToWatch = to_wide_char(ptcFileName);

	dwChangeHandles = FindFirstChangeNotification(
		ptcFileBaseDir,
		FALSE,
		FILE_NOTIFY_CHANGE_FILE_NAME |
		FILE_NOTIFY_CHANGE_DIR_NAME |
		FILE_NOTIFY_CHANGE_ATTRIBUTES |
		FILE_NOTIFY_CHANGE_SIZE |
		FILE_NOTIFY_CHANGE_LAST_WRITE |
		FILE_NOTIFY_CHANGE_LAST_ACCESS |
		FILE_NOTIFY_CHANGE_CREATION |
		FILE_NOTIFY_CHANGE_SECURITY |
		FILE_NOTIFY_CHANGE_CEGETINFO
	);

	if (dwChangeHandles == INVALID_HANDLE_VALUE)
	{
		printf("\n ERROR: FindFirstChangeNotification function failed [%d].\n", GetLastError());
		return;
	}

	while (TRUE)
	{
		// Wait for notification.
		printf("\n\n[%d] Waiting for notification...\n", iCount);
		iCount++;

		dwWaitStatus = WaitForSingleObject(dwChangeHandles, INFINITE);
		switch (dwWaitStatus)
		{
		case WAIT_OBJECT_0:

			printf("Change detected\n");

			DWORD iBytesReturned, iBytesAvaible;
			if (CeGetFileNotificationInfo(dwChangeHandles, 0, NULL, 0, &iBytesReturned, &iBytesAvaible) != 0)
			{
				std::vector< BYTE > vecBuffer(iBytesAvaible);

				if (CeGetFileNotificationInfo(dwChangeHandles, 0, &vecBuffer.front(), vecBuffer.size(), &iBytesReturned, &iBytesAvaible) != 0) {
					BYTE* p_bCurrent = &vecBuffer.front();
					PFILE_NOTIFY_INFORMATION info = NULL;

					do {
						info = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(p_bCurrent);
						p_bCurrent += info->NextEntryOffset;

						if (wszFileNameToWatch.compare(info->FileName) == 0)
						{
							wcout << "\n\t[" << info->FileName << "]: 0x" << ::hex << info->Action;

							switch (info->Action) {
							case FILE_ACTION_ADDED:
								break;
							case FILE_ACTION_MODIFIED:
								break;
							case FILE_ACTION_REMOVED:
								break;
							case FILE_ACTION_RENAMED_NEW_NAME:
								break;
							case FILE_ACTION_RENAMED_OLD_NAME:
								break;
							}
						}
					} while (info->NextEntryOffset != 0);
				}
			}

			if (FindNextChangeNotification(dwChangeHandles) == FALSE)
			{
				printf("\n ERROR: FindNextChangeNotification function failed [%d].\n", GetLastError());
				return;
			}

			break;

		case WAIT_TIMEOUT:
			printf("\nNo changes in the timeout period.\n");
			break;

		default:
			printf("\n ERROR: Unhandled dwWaitStatus [%d].\n", GetLastError());
			return;
			break;
		}
}
*/
