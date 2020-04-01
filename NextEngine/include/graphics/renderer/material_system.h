#pragma once

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include "ecs/id.h"
#include "core/reflection.h"
#include "graphics/rhi/draw.h"
#include "core/container/vector.h"
#include "core/container/string_buffer.h"
#include "core/container/string_view.h"
#include "core/handle.h"

struct ShaderManager;
struct AssetManager;

enum Param_Type {
	Param_Vec3,
	Param_Vec2,
	Param_Mat4x4,
	Param_Image,
	Param_Cubemap,
	Param_Int,
	Param_Float,
	Param_Channel3, Param_Channel2, Param_Channel1, 
	Param_Time
};

struct ENGINE_API Param {
	struct Channel3 {
		texture_handle image;
		uniform_handle scalar_loc;
		glm::vec3 color;

		REFLECT(ENGINE_API)
	};

	struct Channel2 {
		texture_handle image;
		uniform_handle scalar_loc;
		glm::vec2 value;

		REFLECT(ENGINE_API)
	};

	struct Channel1{
		texture_handle image;
		uniform_handle scalar_loc;
		float value;

		REFLECT(ENGINE_API)
	};

	uniform_handle loc;
	Param_Type type;
	union {
		glm::vec3 vec3;
		glm::vec2 vec2;
		glm::mat4 matrix;
		texture_handle image;
		cubemap_handle cubemap;
		int integer;
		float real;

		Channel3 channel3;
		Channel2 channel2;
		Channel1 channel1;

		int time; //pointless makes it easy to serialize
	};

	Param();

	REFLECT_UNION()
};

struct ENGINE_API Material {
	shader_handle shader;
	vector<Param> params;
	DrawCommandState* state = &default_draw_state;

	unsigned int shader_flags = 0;

	void retarget_from(AssetManager&, material_handle);
	void set_flag(string_view, bool);

	
	void set_int(ShaderManager&, string_view, int);
	void set_float(ShaderManager&, string_view, float);
	void set_vec2(ShaderManager&, string_view, glm::vec2);
	void set_vec3(ShaderManager&, string_view, glm::vec3);
	void set_cubemap(ShaderManager&, string_view, cubemap_handle);
	void set_image(ShaderManager&, string_view, texture_handle);
	
	void set_channel3(ShaderManager&, string_view, glm::vec3, texture_handle img = { INVALID_HANDLE });
	void set_channel2(ShaderManager&, string_view, glm::vec2, texture_handle img = { INVALID_HANDLE });
	void set_channel1(ShaderManager&, string_view, float, texture_handle img = { INVALID_HANDLE });

	Material();
	Material(shader_handle);

	REFLECT(NO_ARG)
};

struct Materials {
	vector<material_handle> materials;

	REFLECT(ENGINE_API)
};

vector<Param> make_SubstanceMaterial(string_view folder, string_view);