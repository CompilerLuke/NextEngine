#pragma once

#include "core/core.h"
#include <stdio.h>

template<typename T, int N>
struct Pool {
	union Slot {
		T data;
		Slot* next_free_slot;
	};

	Slot slots[N];
	Slot* free;

	Pool() {
		for (uint i = 0; i < N - 1; i++) {
			slots[i].next_free = &slots[i + 1];
		}

		free = NULL;
		slots[N - 1].next_free = NULL;
	}

	uint index(T* data) {
		return (Slot*)data - slots;
	}

	T* alloc() {
		Slot* ptr = free;
		if (!ptr) {
			printf("Pool is complete! size: %i\n", N);
			abort();
		}

		free = free->next_free;

		return &ptr.data;

		Slot& slot = slots[ptr];
		free = slot.next_free;

		return ptr + 1;
	}

	void dealloc(T* ptr) {
		Slot* slot = (Slot*)ptr;
		slot->next_free = free;
		free = slot;
	}
};