#include "stdafx.h"
#include "core/string_view.h"
#include "core/string_buffer.h"

StringView::StringView(const StringBuffer& buffer) : data(buffer.data == NULL ? "" : buffer.data), length(buffer.length) {

}

bool string_to_uint(StringView str, unsigned int* number) {
	if (str.length == 0) return false;

	bool index = 0;

	if (str.data[index] == '0' && index != str.length - 1) return false;

	int num = 0;
	int multiplier = 1;

	for (; index < str.length; index++) {
		char c = str.data[index];
		if ('0' <= c && c <= '9') {
			num += (c - '0') * multiplier;
			multiplier *= 10;
		}
		else {
			return false;
		}
	}

	int reverse = 0, rem;
	while (num != 0)
	{
		rem = num % 10;
		reverse = reverse * 10 + rem;
		num /= 10;
	}

	*number = reverse;
	return true;
}


bool string_to_int(StringView str, int* number) {
	if (str.length == 0) return false;

	unsigned int num;
	bool success;
	if (str.data[0] == '-') {
		success = string_to_uint(str.sub(1, str.length), &num);
		*number = -(int)num;
	}
	else {
		success = string_to_uint(str, &num);
		*number = num;
	}

	return success;
}



namespace std {
	template <> struct hash<StringView>
	{
		size_t operator()(const StringView & buffer) const
		{
			const char* str = buffer.data;
			unsigned long hash = 5381;
			int c;

			while (c = *str++)
				hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

			return hash;
		}
	};

	template <> struct hash<StringBuffer>
	{
		size_t operator()(const StringBuffer & buffer) const
		{
			return std::hash<StringView>{}({ buffer.data, buffer.length });
		}
	};
}