#pragma once

#include "ecs/id.h"
#include "core/container/tvector.h"

struct Selection {
	ID selected;

	inline bool is_selected(ID id) {
		return selected == id;
	}

	inline void select(ID id) {
		selected = id;
	}

	inline void clear() {
		selected = 0;
	}

	inline tvector<ID> get_selected() {
		tvector<ID> result;
		if (selected > 0) result.append(selected);
		return result;
	}
};
