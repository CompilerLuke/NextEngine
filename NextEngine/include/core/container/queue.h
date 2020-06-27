#pragma once

#include "core/core.h"
#define _ENABLE_ATOMIC_ALIGNMENT_FIX
#include <atomic>

struct HeadAndTail {
	uint head;
	uint tail;
};

template<typename T, int N>
struct queue {
	std::atomic<HeadAndTail> head_and_tail;
	T data[N];

	queue() : head_and_tail({ 0,0 }) {}

	T* try_dequeue() {
		HeadAndTail incr;
		HeadAndTail exp = head_and_tail.load();
		do {
			HeadAndTail exp = head_and_tail.load();
			if (exp.head == exp.tail) return NULL;

			incr = exp;
			incr.tail++;

		} while (!head_and_tail.compare_exchange_weak(exp, incr));

		return &data[exp.tail % N];
	}

	void enqueue(const T& value) {
		HeadAndTail incr;
		HeadAndTail exp;

		do {
			exp = head_and_tail.load();
			incr = exp;
			incr.head++;
		} while (!head_and_tail.compare_exchange_weak(exp, incr));

		if (incr.head - incr.tail > N) {
			printf("QUEUE has exceeded storage %i!", N);
			abort();
		}

		data[exp.head % N] = value;
	}
};