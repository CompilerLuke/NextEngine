#pragma once

template<typename I, typename T>
bool contains(I begin, I end, const T& value) {
	for (I it = begin; it != end; it++) {
		if (*it == value) return true;
	}

	return false;
}