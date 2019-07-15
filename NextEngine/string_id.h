#pragma once

#include <string.h>

using Hash = unsigned long long;
constexpr Hash prime_hash = 17376448622476793053ll;

constexpr Hash hash_string(const char* str) {
	auto h = 0;
	auto o = 31415;
	auto t = 27183;

	unsigned int length = strlen(str);
	for (int i = 0; i < length; i++) {
		h = (o * h + str[i]) % prime_hash;
		o = o * t % (prime_hash - 1);
	}

	return h;
}



struct StringInterning {

};