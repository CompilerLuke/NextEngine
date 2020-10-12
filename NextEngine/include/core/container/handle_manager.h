#pragma once

#include "core/container/array.h"
#include "core/handle.h"

constexpr unsigned int INVALID_SLOT = 0;

template<typename T, typename H, int MAX_HANDLES = 200, int RESERVED_HANDLES = 10>
struct HandleManager {
	unsigned int generation_counter = 0;
	
	array<MAX_HANDLES, unsigned int> free_serialized_ids;
	array<MAX_HANDLES, unsigned int > free_ids;
	array<MAX_HANDLES, T> slots;
	array<MAX_HANDLES, unsigned int> id_of_slots;
	T* by_handle[MAX_HANDLES];

	unsigned int handle_to_index(H handle) {
		return handle.id - 1;
	}

	H assign_handle(T&& obj, bool serialized = false) {
		slots.append(std::move(obj));
		
		unsigned int handle = serialized ? free_serialized_ids.pop() : free_ids.pop();

		id_of_slots.append(handle);
		by_handle[handle- 1] = &slots[slots.length - 1];
		return { handle };
	}

	T* assign_handle(H handle, T&& obj) {
		bool found = false;

		for (int i = 0; i < free_serialized_ids.length; i++) {
			if (free_serialized_ids[i] == handle.id) {
				auto popped = free_serialized_ids.pop();
				if (i != free_serialized_ids.length) free_serialized_ids[i] = popped; //poped is the element we are trying to delete
				found = true;
			}
		}
		
		//assert(found); skybox material is not in the correct range

		slots.append(std::move(obj));

		T* ptr = &slots[slots.length - 1];

		id_of_slots.append(handle.id);
		by_handle[handle.id - 1] = ptr;
		return ptr;
	}

	H index_to_handle(unsigned index) {
		return { id_of_slots[index] };
	}

	T* get(H handle) {
		if (handle.id == INVALID_HANDLE || handle.id > MAX_HANDLES) return NULL;
		return by_handle[handle.id - 1];
	}

	void free(H handle) { 
		unsigned int index = handle_to_index(handle);
		auto last_id = id_of_slots.pop();

		if (index != slots.length) { //poped is the element we are trying to delete
			slots[index].~T(); //todo not sure of this is a leak
			new (&slots[index]) T(std::move(slots.pop())); //Move assign doesnt work on Material
			by_handle[last_id - 1] = &slots[index];
			id_of_slots[index] = last_id;
		}
		else {
			slots.pop();
		}
		by_handle[handle.id - 1] = NULL;
	}

	HandleManager() {
		for (int i = 0; i < MAX_HANDLES; i++) {
			by_handle[i] = NULL;
		}

		for (int i = MAX_HANDLES; i > MAX_HANDLES - 100; i--) { //todo add more flexibility
			free_serialized_ids.append(i);
		}

		for (int i = MAX_HANDLES - 100; i > RESERVED_HANDLES; i--) {
			free_ids.append(i);
		}
	}
};

