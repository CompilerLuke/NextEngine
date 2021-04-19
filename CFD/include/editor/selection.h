#pragma once

#include "ecs/id.h"
#include "core/container/vector.h"
#include "cfd_components.h"

class EditorSelection {
	vector<uint> selected;

public:
	inline void clear() { selected.clear(); }
	inline slice<uint> get_selected() { return selected; }
	inline void select(uint id) { selected.clear(); selected.append(id); }
	inline bool is_active(uint id) { return selected.contains(id); }
	inline bool active() { return selected.length > 0; }
	inline uint get_active() { return selected[0]; }

	inline void toggle(uint id) {
		uint found = 0;
		for (uint idx = 0; idx < selected.length; idx++) {
			if (selected[idx] == id) found = true;
			else selected[idx - found] = selected[idx];
		}
		if (found) selected.length--;
		else selected.append(id);
	}

	inline void append(uint id) { selected.append(id); }
};


struct SceneSelection : EditorSelection {

};

enum class MeshPrimitive { Vertex, Edge, Triangle };

struct InputMeshSelection : EditorSelection {
	MeshPrimitive mode = MeshPrimitive::Edge;
	input_model_handle model;
};
