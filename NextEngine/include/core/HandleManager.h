#pragma once

#include <vector>
#include "handle.h"
#include "reflection/reflection.h"

constexpr unsigned int INVALID_SLOT = 0;

template<typename T>
struct HandleManager {
	unsigned int generation_counter = 0;

	struct Slot {
		union {
			struct {
				unsigned int generation;
				T obj;
			};
			Handle<T> next_free_slot;
		};

		~Slot() {}
	};
	
	vector<Slot> slots;
	Handle<T> next_free_slot = { INVALID_SLOT };

	unsigned int next_generation() {
		unsigned int generation = generation_counter++;
		generation_counter %= (2 << 11);
		return generation + 1;
	}

	Handle<T> make(T&& obj) {
		unsigned int generation = next_generation();
		unsigned int next_free_slot = this->next_free_slot.id;

		if (next_free_slot == INVALID_SLOT) {
			slots.reserve(slots.length + 1);
			slots.length++;

			next_free_slot = slots.length;
			slots[next_free_slot - 1].next_free_slot = { INVALID_SLOT };
		}

		Slot& slot = slots[next_free_slot - 1];

		this->next_free_slot = slot.next_free_slot;
		new (&slot.obj) T(std::move(obj));
		slot.generation = 1; //generation;

		return index_to_handle(next_free_slot - 1);
	}

	Handle<T> index_to_handle(unsigned index) {
		return { index + 1 };
		return { (slots[index].generation << 19) | index + 1 };
	}

	unsigned get_index(Handle<T> handle) {
		return handle.id - 1;
		//return (handle.id & ((1 << 20) - 1)) - 1; //1- since the index is shifted to support 0 being an invalid state
	}

	unsigned int get_generation(Handle<T> handle) {
		return 1;
		return handle.id >> 19;
	}

	T* get(Handle<T> handle) {
		unsigned int index = get_index(handle);
		unsigned int generation = get_generation(handle);

		if (index >= slots.length) return NULL;

		Slot* slot = &slots[index];
		
		if (slot->generation == generation) return &slot->obj;
		else return NULL;
	}

	void free(Handle<T> handle) {
		unsigned int index = get_index(handle);
		unsigned int generation = get_generation(handle);

		if (index >= slots.length) return;

		Slot* slot = &slots[index];

		if (slot->generation == generation) {
			slot->generation = INVALID_SLOT;
			slot->obj.~T();
		}
	}

	~HandleManager() {
		for (int i = 0; i < slots.length; i++) {
			if (slots[i].generation != INVALID_SLOT) slots[i].obj.~T();
		}
	}
};

