#include "stdafx.h"
#include "logger/logger.h"
#include <iostream>

std::string format(std::string& str) {
	return str;
}

std::string format(const char* str) {
	return std::string(str);
}

std::string format(int num) {
	return std::to_string(num);
}

std::string format(unsigned int num) {
	return std::to_string(num);
}

void log_string(const std::string& s) {
	std::cout << s << std::endl;
}

void log(const char* s) {
	std::cout << s << "\n";
}