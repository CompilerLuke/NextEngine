#pragma once

#include "core/core.h"
#include "core/memory/allocator.h"
#include "core/container/string_buffer.h"
#include "core/container/string_view.h"
#include "core/container/sstring.h"

void ENGINE_API format_intern(string_buffer&, char* str);
void ENGINE_API format_intern(string_buffer&, const char* str);
void ENGINE_API format_intern(string_buffer&, string_view str);
void ENGINE_API format_intern(string_buffer&, sstring str);
void ENGINE_API format_intern(string_buffer&, int num);
void ENGINE_API format_intern(string_buffer&, float num);
void ENGINE_API format_intern(string_buffer&, unsigned int num);
void ENGINE_API format_intern(string_buffer&, const string_buffer&);

template<typename T>
void format_intern(string_buffer&, T arg) {
	static_assert(false, "unsupported format type");
}

template<typename T, typename ...Args>
void format_intern(string_buffer& buffer, T arg, Args... args) {
	format_intern(buffer, arg);
	format_intern(buffer, args...);
}

template<typename ...Args>
string_buffer format(Args... args) {
	string_buffer buffer;
	format_intern(buffer, args...);
	return buffer;
}

template<typename ...Args>
string_buffer tformat(Args... args) {
	string_buffer buffer;
	buffer.allocator = &temporary_allocator;
	format_intern(buffer, args...);
	return buffer;
}

void ENGINE_API log_string(string_view);

void ENGINE_API log(const char*);

template<typename ...Args>
void log(Args... args) {
	log_string(format(args...));
}

ENGINE_API void flush_logger();