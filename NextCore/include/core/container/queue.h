#pragma once

#include "core/core.h"
#include <atomic>

template<typename T, uint N>
struct queue {
	T data[N];
	std::atomic<uint> laps[N] = {};

	std::atomic<u64> head = {};
	std::atomic<u64> tail = {};

	queue() {
		head = 0;
		tail = 1ull << 32;
	}

	uint length() {
		uint head = this->head.load() >> 32;
		uint tail = this->tail.load() >> 32;
		
		return (tail - head) % N;
	}

	bool enqueue(T&& element) {
		u64 head = this->head.load(std::memory_order_relaxed);

		while (true) {
			uint head_lap = head >> 32;
			uint index = head & ((1ull << 32) - 1);

			uint e_lap = laps[index].load(std::memory_order_relaxed);

			if (e_lap == head_lap) {
				u64 new_head = index + 1 == N ? (u64)(head_lap + 2) << 32 : index + 1 | (u64)head_lap << 32;

				if (this->head.compare_exchange_strong(head, new_head, std::memory_order_acquire)) {
					data[index] = std::move(element);
					laps[index].store(head_lap + 1, std::memory_order_release);

					return true;
				}
			}
			else if ((int)head_lap - (int)e_lap > 0) {
				//read has not completed yet or queue is full
				return false;
			}
			else {
				//element was written before we got there, try again
				head = this->head.load(std::memory_order_relaxed);
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
				return false;
			}
			else { 
				//element was read before we got there, try again
				tail = this->tail.load(std::memory_order_relaxed);
				continue;
			}
		}
	}
};
