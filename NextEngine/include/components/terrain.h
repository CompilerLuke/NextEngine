#pragma once

#include "core/core.h"
#include "core/handle.h"

COMP
struct TerrainControlPoint {};

REFL
struct TerrainMaterial {
	texture_handle diffuse;
	texture_handle metallic;
	texture_handle roughness;
	texture_handle normal;
	texture_handle height;
	texture_handle ao;
};

struct pixel {
	union {
		struct {
			char r;
			char g;
			char b;
			char a;
		};

		uint packed;
	};
};

COMP
struct Terrain {
	uint width = 12;
	uint height = 12;
	uint size_of_block = 10;
	bool show_control_points;
	texture_handle heightmap;

	vector<float> displacement_map;
	vector<pixel> blend_idx_map;
	vector<pixel> blend_values_map;

	float max_height = 10.0f;

	vector<TerrainMaterial> materials;
};

float ENGINE_API barry_centric(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec2 pos);
float ENGINE_API sample_terrain_height(Terrain& terrain, struct Transform& terrain_trans, glm::vec2 position);