layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

layout (std140, set = 0, binding = 0) uniform PassUBO {
	vec2 resolution;
	mat4 projection;
	mat4 view;
};

layout (std140, push_constant) uniform UBO {
    mat4 model;
};

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);

}