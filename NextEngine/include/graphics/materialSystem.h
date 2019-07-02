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

enum Param_Type {
	Param_Vec3,
	Param_Vec2,
	Param_Mat4x4,
	Param_Image,
	Param_Cubemap,
	Param_Int
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
	};

	Param();

	REFLECT_UNION()
};

Param make_Param_Int(Handle<Uniform> loc, int);
Param make_Param_Vec3(Handle<Uniform> loc, glm::vec3);
Param make_Param_Cubemap(Handle<Uniform> loc, Handle<struct Cubemap>);

struct Material {
	std::string name;
	Handle<Shader> shader;
	vector<Param> params;
	DrawCommandState* state;

	REFLECT()
};

struct Materials {
	vector<Material> materials;

	REFLECT()
};

Material* material_by_name(vector<Material>&, const std::string&);
vector<Param> make_SubstanceMaterial(World& world, const std::string& folder, const std::string& name);