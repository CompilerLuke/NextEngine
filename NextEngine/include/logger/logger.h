#pragma once

#include <string>

std::string format(std::string& str);
std::string format(const char* str);
std::string format(int num);
std::string format(unsigned int num);

template<typename T, typename ...Args>
std::string format(T arg, Args... args) {
	return format(arg) + format(args...);
}

void log_string(const std::string&);

template<typename ...Args>
void log(Args... args) {
	log_string(format(args...));
}
