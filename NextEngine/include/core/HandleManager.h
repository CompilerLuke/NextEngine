#pragma once

#include <vector>
#include "handle.h"
#include "reflection/reflection.h"

constexpr unsigned int INVALID_SLOT = 0;

template<typename T>
struct HandleManager {
	unsigned int generation_counter = 0;
	vector<unsigned int> free_serialized_ids;
	vector<unsigned int> free_ids;

	static constexpr unsigned int MAX_HANDLES = 200;

	vector<T> slots;
	vector<unsigned int> id_of_slots;
	T* by_handle[MAX_HANDLES];

	unsigned int handle_to_index(Handle<T> handle) {
		return handle.id - 1;
	}

	Handle<T> make(T&& obj, bool serialized = false) {
		slots.append(std::move(obj));
		
		unsigned int handle = serialized ? free_serialized_ids.pop() : free_ids.pop();

		id_of_slots.append(handle);
		by_handle[handle- 1] = &slots[slots.length - 1];
		return { handle };
	}

	void make(Handle<T> handle, T&& obj) {
		for (int i = 0; i < free_serialized_ids.length; i++) {
			if (free_serialized_ids[i] == handle.id) {
				auto popped = free_serialized_ids.pop();
				if (i != free_serialized_ids.length) free_serialized_ids[i] = popped; //poped is the element we are trying to delete
			}
		}
		
		slots.append(std::move(obj));

		id_of_slots.append(handle.id);
		by_handle[handle.id - 1] = &slots[slots.length - 1];
	}

	Handle<T> index_to_handle(unsigned index) {
		return { id_of_slots[index] };
	}

	T* get(Handle<T> handle) {
		if (handle.id == INVALID_HANDLE || handle.id > MAX_HANDLES) return NULL;
		return by_handle[handle.id - 1];
	}

	void free(Handle<T> handle) {
		unsigned int index = handle_to_index(handle);
		auto last_id = id_of_slots.pop();
		auto popped = slots.pop();

		if (index != slots.length) { //poped is the element we are trying to delete
			slots[index] = std::move(popped);
			by_handle[last_id - 1] = &slots[index];
		}
		by_handle[handle.id - 1] = NULL;
	}

	HandleManager() {
		for (int i = 0; i < MAX_HANDLES; i++) {
			by_handle[i] = NULL;
		}

		for (int i = MAX_HANDLES; i > MAX_HANDLES - 100; i--) { //todo add more flexibility
			free_ids.append(i);
		}

		for (int i = MAX_HANDLES - 100; i > 0; i--) {
			free_serialized_ids.append(i);
		}

		slots.reserve(MAX_HANDLES);
	}
};

