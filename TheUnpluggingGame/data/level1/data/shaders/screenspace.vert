layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

layout (binding = 0) uniform UBO {
    mat4 model;
    vec3 viewPos;
} ubo;

layout(location = 0) out vec2 TexCoords;
layout(location = 1) out vec3 NDC;

void main()
{
    gl_Position = ubo.model * vec4(aPos, 1.0);
    TexCoords = aTexCoords;

    NDC = (gl_Position / gl_Position.w).xyz;
}