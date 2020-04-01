#include "stdafx.h"
#include "core/io/vfs.h"
#include <fstream>
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <functional>

void Level::set(string_view asset_folder_path) {
	this->asset_folder_path = asset_folder_path;
}

string_buffer Level::asset_path(string_view filename) {
	string_buffer buffer;
	buffer.allocator = &temporary_allocator;
	buffer += asset_folder_path;
	buffer += filename;
	return buffer;
}

bool Level::to_asset_path(string_view filename, string_buffer* result) {
	if (filename.starts_with(asset_folder_path)) {
		*result = filename.sub(asset_folder_path.size(), filename.size());
		return true;
	}

	return false;
}

File::File(Level& level) : level(level) {}

bool File::open(string_view filename, FileMode mode) {
	this->filename = gb::level.asset_path(filename);

	const char* c_mode = NULL;
	if (mode == FileMode::ReadFile) c_mode = "r";
	if (mode == FileMode::WriteFile) c_mode = "w";
	if (mode == FileMode::ReadFileB) c_mode = "rb";
	if (mode == FileMode::WriteFileB) c_mode = "wb";

	errno_t errors = fopen_s(&this->f, this->filename.c_str(), c_mode);
	return this->f != NULL && !errors;
}

void File::close() {
	if (this->f) fclose(this->f);
	this->f = NULL;
}

File::~File() {
	close();
}

string_buffer File::read() {
	struct _stat info[1];

	_stat(filename.c_str(), info);
	size_t length = info->st_size;

	string_buffer buffer;
	buffer.reserve(length + 1);
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

void File::write(string_view content) {
	fwrite(content.data, sizeof(char), sizeof(char) * content.size(), f);
}

long long Level::time_modified(string_view filename) {
	auto f = asset_path(filename);
	
	struct _stat buffer;
	if (_stat(f.c_str(), &buffer) != 0) {
		throw string_buffer("Could not read file ") + f;
	}
	return buffer.st_mtime;
}

wchar_t* to_wide_char(const char* orig);

void WatchFileChange(string_view file, std::function<void()> func) {
	/*

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
	*/
}