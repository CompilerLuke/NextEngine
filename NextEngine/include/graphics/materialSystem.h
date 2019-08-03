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
	Param_Float
};

struct Param {
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
	};

	Param();

	REFLECT_UNION()
};

Param make_Param_Int(Handle<Uniform> loc, int);
Param make_Param_Float(Handle<Uniform> loc, float);
Param make_Param_Vec2(Handle<Uniform> loc, glm::vec2);
Param make_Param_Vec3(Handle<Uniform> loc, glm::vec3);
Param make_Param_Cubemap(Handle<Uniform> loc, Handle<struct Cubemap>);
Param make_Param_Image(Handle<Uniform> loc, Handle<struct Texture>);

struct Material {
	Handle<Shader> shader;
	vector<Param> params;
	DrawCommandState* state = &default_draw_state;

	REFLECT()
};

struct Materials {
	vector<Handle<Material>> materials;

	REFLECT()
};

vector<Param> make_SubstanceMaterial(StringView folder, StringView);