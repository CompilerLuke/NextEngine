#pragma once

#include "core/core.h"
#include "core/allocator.h"
#include <string>
#include "core/string_buffer.h"
#include "core/string_view.h"

void ENGINE_API format_intern(StringBuffer&, char* str);
void ENGINE_API format_intern(StringBuffer&, const char* str);
void ENGINE_API format_intern(StringBuffer&, StringView str);
void ENGINE_API format_intern(StringBuffer&, int num);
void ENGINE_API format_intern(StringBuffer&, float num);
void ENGINE_API format_intern(StringBuffer&, unsigned int num);
void ENGINE_API format_intern(StringBuffer&, const StringBuffer&);

template<typename T>
void format_intern(StringBuffer&, T arg) {
	static_assert(false, "unsupported format type");
}

template<typename T, typename ...Args>
void format_intern(StringBuffer& buffer, T arg, Args... args) {
	format_intern(buffer, arg);
	format_intern(buffer, args...);
}

template<typename ...Args>
StringBuffer format(Args... args) {
	StringBuffer buffer;
	format_intern(buffer, args...);
	return buffer;
}

template<typename ...Args>
StringBuffer tformat(Args... args) {
	StringBuffer buffer;
	buffer.allocator = &temporary_allocator;
	format(buffer, args...);
	return buffer;
}

void ENGINE_API log_string(StringView);

void ENGINE_API log(const char*);

template<typename ...Args>
void log(Args... args) {
	log_string(format(args...));
}
