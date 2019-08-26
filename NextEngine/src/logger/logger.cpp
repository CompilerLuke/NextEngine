#include "stdafx.h"
#include "logger/logger.h"
#include <iostream>

void format_intern(StringBuffer& buffer, const StringBuffer& str) {
	buffer += str;
}


void format_intern(StringBuffer& buffer, StringView str) {
	buffer += str;
}

void format_intern(StringBuffer& buffer, char* str) {
	buffer += (const char*)str;
}

void format_intern(StringBuffer& buffer, const char* str) {
	buffer += str;
}

void format_intern(StringBuffer& buffer, int num) {
	buffer += std::to_string(num).c_str();
}

void format_intern(StringBuffer& buffer, float num) {
	buffer += std::to_string(num).c_str();
}

void format_intern(StringBuffer& buffer, unsigned int num) {
	buffer += std::to_string(num).c_str();
}

StringBuffer log_buffer;

void log_string(StringView s) {
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
	log_string(StringView(s));
}
