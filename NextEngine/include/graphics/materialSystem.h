#pragma once

#include "shader.h"
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include "ecs/id.h"
#include "reflection/reflection.h"
#include "graphics/draw.h"
#include "core/vector.h"
#include "core/handle.h"
#include "core/string_buffer.h"
#include "core/string_view.h"

enum Param_Type {
	Param_Vec3,
	Param_Vec2,
	Param_Mat4x4,
	Param_Image,
	Param_Cubemap,
	Param_Int,
	Param_Float,
	Param_Channel3, Param_Channel2, Param_Channel1
};

struct Param {
	struct Channel3 {
		Handle<struct Texture> image;
		Handle<Uniform> scalar_loc;
		glm::vec3 color;

		REFLECT()
	};

	struct Channel2 {
		Handle<struct Texture> image;
		Handle<Uniform> scalar_loc;
		glm::vec2 value;

		REFLECT()
	};

	struct Channel1{
		Handle<struct Texture> image;
		Handle<Uniform> scalar_loc;
		float value;

		REFLECT()
	};

	Handle<Uniform> loc;
	Param_Type type;
	union {
		glm::vec3 vec3;
		glm::vec2 vec2;
		glm::mat4 matrix;
		Handle<struct Texture> image;
		Handle<struct Cubemap> cubemap;
		int integer;
		float real;

		Channel3 channel3;
		Channel2 channel2;
		Channel1 channel1;
	};

	Param();

	REFLECT_UNION()
};

struct Material {
	Handle<Shader> shader;
	vector<Param> params;
	DrawCommandState* state = &default_draw_state;

	unsigned int shader_flags = 0;

	void retarget_from(Handle<Material>);


	void set_flag(StringView, bool);

	void set_int(StringView, int);
	void set_float(StringView, float);
	void set_vec2(StringView, glm::vec2);
	void set_vec3(StringView, glm::vec3);
	void set_cubemap(StringView, Handle<struct Cubemap>);
	void set_image(StringView, Handle<struct Texture>);
	
	void set_channel3(StringView, glm::vec3, Handle<struct Texture> img = { INVALID_HANDLE });
	void set_channel2(StringView, glm::vec2, Handle<struct Texture> img = { INVALID_HANDLE });
	void set_channel1(StringView, float, Handle<struct Texture> img = { INVALID_HANDLE });

	Material();
	Material(Handle<Shader>);

	REFLECT()
};

struct Materials {
	vector<Handle<Material>> materials;

	REFLECT()
};

vector<Param> make_SubstanceMaterial(StringView folder, StringView);