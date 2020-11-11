#pragma once

#include "core/core.h"
#include "core/atomic.h"
#include <atomic>

template<uint N, typename T>
struct work_stealing_queue {
	T data[N] = {};
	std::atomic<uint> top = 0;
	int bottom = 0;

	inline bool push(T item) {
		//TODO: assert that this is only called by owning thread
		uint index = bottom;
		data[index % N] = item;

		TASK_COMPILER_BARRIER

		bottom = index + 1;
		return true;
	}

	inline bool pop(T* result) {
		int bottom = this->bottom - 1;
		this->bottom = bottom;

		TASK_MEMORY_BARRIER

		uint top = this->top;
		if ((int)top <= bottom) {
			T elem = data[bottom % N];

			if (top != bottom) {
				// still >0 jobs left in the queue
				*result = elem;
				return true;
			}
			else {
				uint copy_top = top;
				bool empty = !this->top.compare_exchange_strong(copy_top, top + 1);
					
				//data[bottom % N] = {};
				this->bottom = top + 1;
					
				if (!empty) {
					*result = elem;
					return true;
				}
				return false;
			}
		}
		else {
			//already empty
			this->bottom = top;
			return false;
		}
	}

	inline bool steal(T* result) {
		uint top = this->top;

		//Ensure top is always read before bottom
		//A LoadLoad memory barrier would also be necessary on platforms with a weak memory model
		TASK_COMPILER_BARRIER;

		int bottom = this->bottom;

		if ((int)top < bottom) {
			T elem = data[top % N];

			if (!this->top.compare_exchange_strong(top, top + 1)) {
				return false;
			}

			data[top % N] = {};
			*result = elem;
			return true;
		}

		return false;
	}
};
