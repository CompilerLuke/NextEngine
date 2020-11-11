#pragma once

#include "core/core.h"
#include "core/memory/linear_allocator.h"
#include "core/container/string_buffer.h"
#include "core/container/string_view.h"
#include "core/container/sstring.h"
#include <stdlib.h>
#include <stdio.h>

//can't we simply accept a string_view?
inline void format_intern(string_buffer& buffer, char* str) {
	buffer += str;
}

inline void format_intern(string_buffer& buffer, const char* str) {
	buffer += str;
}

inline void format_intern(string_buffer& buffer, string_view str) {
	buffer += str;
}

inline void format_intern(string_buffer& buffer, sstring str) {
	buffer += str;
}

inline void format_intern(string_buffer& buffer, int num) {
	char as_str[16];
    snprintf(as_str, 16, "%d", num);
	buffer += as_str;
}

inline void format_intern(string_buffer& buffer, float num) {
	char as_str[32];
	snprintf(as_str, 32, "%f", num);
	buffer += as_str;
}

inline void format_intern(string_buffer& buffer, uint num) {
	format_intern(buffer, (int)num);
}

inline void format_intern(string_buffer& buffer, u64 num) {
	char as_str[32];
    snprintf(as_str, 32, "%llu", num);
	buffer += as_str;
}

inline void format_intern(string_buffer& buffer, const string_buffer& str) {
	buffer += str;
}

template<typename T>
void format_intern(string_buffer&, T arg) {
	//static_assert(false, "unsupported format type");
}

template<typename ...Args>
string_buffer format(Args... args) {
	string_buffer buffer;
	(format_intern(buffer, args), ...);
	return buffer;
}

template<typename ...Args>
string_buffer tformat(Args... args) {
	string_buffer buffer;
	buffer.allocator = &get_temporary_allocator();
	(format_intern(buffer, args), ...);
	return buffer;
}

template<typename ...Args>
string_buffer tformat(string_view a, string_view b) {
	string_buffer buffer;
	buffer.allocator = &get_temporary_allocator();
	buffer.length = a.length + b.length;
	buffer.capacity = buffer.length;
	buffer.data = TEMPORARY_ARRAY(char, buffer.capacity + 1);
	memcpy(buffer.data, a.data, a.length);
	memcpy(buffer.data + a.length, b.data, b.length);
	buffer.data[buffer.length] = '\0';

	return buffer;
}

void CORE_API log_string(string_view);

void CORE_API log(const char*);

template<typename ...Args>
void log(Args... args) {
	log_string(format(args...));
}

CORE_API void flush_logger();

enum LogLevel {
	LOG_LEVEL_DEBUG,
	LOG_LEVEL_INFO,
	LOG_LEVEL_WARNING,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_CRITICAL
};

struct Logger {
	virtual void log_string(LogLevel log_level, string_view str) = 0;
	virtual void flush() = 0;
};

struct DefaultLogger : Logger {
	LogLevel min_log_level;

	CORE_API void log_string(LogLevel log_level, string_view str) override;
	CORE_API void flush() override;
};

struct DisabledLogger : Logger {
	CORE_API void log_string(LogLevel log_level, string_view str) override;
	CORE_API void flush() override;
};
