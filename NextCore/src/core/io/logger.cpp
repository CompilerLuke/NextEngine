#include "stdafx.h"
#include "core/io/logger.h"
#include <stdio.h>

string_buffer log_buffer;

void log_string(string_view s) {
	//fwrite(s.c_str(), s.length, 1, stdout);
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

void DefaultLogger::log_string(LogLevel log_level, string_view str) {
	if (log_level < min_log_level) return;

	fwrite(str.data, str.length, 1, stdout);
}

void DefaultLogger::flush() {
	fflush(stdout);
}

void DisabledLogger::log_string(LogLevel log_level, string_view str) {

}

void DisabledLogger::flush() {

}
