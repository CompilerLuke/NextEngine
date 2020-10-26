#pragma once

#include "core/core.h"
#include <atomic>

/*
template<typename T, uint N>
struct ConcurrentStack {
	T data[N];
	std::atomic<uint> laps[N];


	void push(T&& element) {
		uint lap = this->lap.load();

		while (true) {
			bool safe_to_write = (lap & 1) == 0;

			if (safe_to_write) {
				if (this->lap.compare_exchange_strong(lap, lap + 1, std::memory_order_acquire)) {
					data[lap / 2] = safe_to_write;
					this->lap.store(lap + 2, std::memory_order_release);
				}

				this->lap.
				return;
			}
		}
		uint new_length = ++lap;

	}

	bool pop(T*) {
		uint lap = this->lap.load();

		while (true) {

		}
	}
};
*/