#pragma once

#include "engine/core.h"
#include "engine/handle.h"
#include "core/container/sstring.h"
#include "core/container/vector.h"
#include <glm/glm.hpp>

COMP
struct TerrainControlPoint {};

COMP
struct TerrainSplat { 
	float hardness = 0.5f;
	float min_height = 0.0f;
	float max_height = 100.0f;
	uint material = 1; 
};

REFL
struct TerrainMaterial {
	sstring name;
	texture_handle diffuse;
	texture_handle metallic;
	texture_handle roughness;
	texture_handle normal;
	texture_handle height;
	texture_handle ao;
};

inline uint pack_char_rgb(u8 r, u8 g, u8 b, u8 a) {
	union {
		struct { u8 r, g, b, a; };
		uint packed;
	} pixel = {r,g,b,a};

	return pixel.packed;
}

inline uint pack_float_rgb(float r, float g, float b, float a) {
	return pack_char_rgb(r * 255, g * 255, b * 255, a * 255);
}

const uint TERRAIN_RESOLUTION = 32;

COMP
struct Terrain {
	uint width = 12;
	uint height = 12;
	uint size_of_block = 10;
	bool show_control_points = true;
	texture_handle heightmap;

	REFL_FALSE vector<float> displacement_map[3];
	REFL_FALSE vector<uint> blend_idx_map;
	REFL_FALSE vector<uint> blend_values_map;

	float max_height = 50.0f;

	vector<TerrainMaterial> materials;
};

float ENGINE_API barry_centric(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec2 pos);
float ENGINE_API sample_terrain_height(Terrain& terrain, struct Transform& terrain_trans, glm::vec2 position);