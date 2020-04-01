#include "stdafx.h"
#include "core/io/logger.h"
#include <string>

void format_intern(string_buffer& buffer, const string_buffer& str) {
	buffer += str;
}


void format_intern(string_buffer& buffer, string_view str) {
	buffer += str;
}

void format_intern(string_buffer& buffer, char* str) {
	buffer += (const char*)str;
}

void format_intern(string_buffer& buffer, const char* str) {
	buffer += str;
}

void format_intern(string_buffer& buffer, sstring str) {
	buffer += str;
}

void format_intern(string_buffer& buffer, int num) {
	buffer += std::to_string(num).c_str();
}

void format_intern(string_buffer& buffer, float num) {
	buffer += std::to_string(num).c_str();
}

void format_intern(string_buffer& buffer, unsigned int num) {
	buffer += std::to_string(num).c_str();
}

string_buffer log_buffer;

void log_string(string_view s) {
	fwrite(s.c_str(), s.length, 1, stdout);
	return;

	log_buffer += s;
	log_buffer += "\n";

	if (log_buffer.length > 10000) {
		fwrite(log_buffer.c_str(), log_buffer.length, 1, stdout);
		log_buffer.length = 0;
	}
}

void flush_logger() {
	fwrite(log_buffer.c_str(), log_buffer.length, 1, stdout);
	log_buffer.length = 0;

	fflush(stdout);
}

void log(const char* s) {
	log_string(string_view(s));
}
