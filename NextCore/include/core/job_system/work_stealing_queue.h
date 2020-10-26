#pragma once

/*
#include "core/core.h"
#include <atomic>

template<uint N, typename T>
struct work_stealing_queue {
	T data[N];
	std::atomic<uint> laps[N];

	u64 head;
	std::atomic<u64> tail;

	work_stealing_queue() {
		head = 0;
		tail = 1ull << 32;
	}

	uint length() {
		uint head = this->head.load() >> 32;
		uint tail = this->tail.load() >> 32;

		return (tail - head) % N;
	}

	bool enqueue(T&& element) {
		while (true) {
			uint head_lap = head >> 32;
			uint index = head & ((1ull << 32) - 1);

			uint e_lap = laps[index].load(std::memory_order_acquire);

			if (e_lap == head_lap) {
				u64 new_head = index + 1 == N ? (u64)(head_lap + 2) << 32 : index + 1 | (u64)head_lap << 32;
				head = new_head;
				
				data[index] = std::move(element);
				laps[index].store(head_lap + 1, std::memory_order_release);

				return true;
			}
			else if ((int)head_lap - (int)e_lap > 0) {
				//read has not completed yet or queue is full
				return nullptr;
			}
			else {
				//element was written before we got there, try again
				continue;
			}
		}
	}

	bool dequeue(T* result) {
		u64 tail = this->tail.load(std::memory_order_acquire);

		while (true) {
			uint tail_lap = tail >> 32;
			uint index = tail & ((1ull << 32) - 1);

			uint e_lap = laps[index].load(std::memory_order_relaxed);

			assert(tail_lap != 0);

			if (e_lap == tail_lap) {
				u64 new_tail = index + 1 == N ? (u64)(tail_lap + 2) << 32u : (u64)index + 1 | (u64)tail_lap << 32;
				u64 prev_tail = tail;

				if (this->tail.compare_exchange_strong(tail, new_tail, std::memory_order_acquire)) { //could this be relaxed memory order
					uint new_tail_lap = new_tail >> 32;
					uint new_index = new_tail & ((1ull << 32) - 1);

					assert(new_tail_lap != 0);

					*result = std::move(data[index]);
					laps[index].store(tail_lap + 1, std::memory_order_release);

					return true;
				}
			}
			else if ((int)tail_lap - (int)e_lap > 0) {
				//write has not completed yet or queue is empty
				return nullptr;
			}
			else {
				//element was read before we got there, try again
				tail = this->tail.load(std::memory_order_relaxed);
				continue;
			}
		}
	}
};
*/


#include "core/core.h"
#include <atomic>

#ifdef _MSC_VER
#	include <windows.h>
#	define TASK_YIELD() YieldProcessor()
#	define TASK_COMPILER_BARRIER _ReadWriteBarrier();
#	define TASK_MEMORY_BARRIER std::atomic_thread_fence(std::memory_order_seq_cst);
#else
#	include <emmintrin.h>
#	define TASK_YIELD() _mm_pause()
#	define TASK_COMPILER_BARRIER asm volatile("" ::: "memory")
#	define TASK_MEMORY_BARRIER asm volatile("mfence" ::: "memory")
#endif 

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
