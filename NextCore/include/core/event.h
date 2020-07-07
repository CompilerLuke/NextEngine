#pragma once

#include "ecs/id.h"
#include "core/container/tvector.h"

enum ComponentEditType {
	EDIT_GRASS_PLACEMENT,
	EDIT_RIGID_BODY_SETTINGS,
	EDIT_TERRAIN_SETTINGS,
	EDIT_MOVED_STATIC_ENTITY,
};

struct EditGrassPlacement {
	ID id;
};

struct EditTerrainSettings {
	ID id;
};

struct MovedStaticEntity {
	ID id;
};

struct ComponentEdit {
	ComponentEditType type;
	ID id;

	glm::vec3 position_delta;
};

struct Event {
	enum Type {
		COMPONENT_EDIT
	} type;
	
	union {
		ComponentEdit component;
	};
};