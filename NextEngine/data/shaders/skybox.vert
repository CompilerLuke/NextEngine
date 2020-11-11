layout (location = 0) in vec3 aPos; 
layout (location = 1) in vec3 aNormal; 
layout (location = 2) in vec2 aTexCoords; 
layout (location = 3) in vec3 aTangent; 
layout (location = 4) in vec3 aBitangent; 
layout (location = 5) in mat4 model;

layout (location = 0) out vec3 localPos;

layout (std140, set = 0, binding = 0) uniform PassUBO {
	vec2 resolution;
	mat4 projection;
	mat4 view;
};

layout (std140, set = 0, binding = 1) uniform SimulationUBO {
	float time;
};

void main()
{
    localPos = aPos;

    mat4 rotView = mat4(mat3(view)); // remove translation from the view matrix
    vec4 clipPos = projection * rotView * vec4(aPos, 1.0);

    gl_Position = clipPos.xyww;
}