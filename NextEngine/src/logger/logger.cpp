#include "stdafx.h"
#include "logger/logger.h"
#include <stdio.h>

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

void format_intern(StringBuffer& buffer, unsigned int num) {
	buffer += std::to_string(num).c_str();
}

void log_string(StringView s) {
	printf("%s\n", s.c_str());
}

void log(const char* s) {
	printf("%s\n", s);
}