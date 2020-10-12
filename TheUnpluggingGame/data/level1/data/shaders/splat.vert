layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;
layout (location = 5) in mat4 model;

layout (location = 0) out vec3 Center;
layout (location = 1) out vec3 FragPos; 
layout (location = 2) out vec2 TexCoords;

layout (set = 0, binding = 0) uniform UBO {
    mat4 proj_view;
};

void main() {
    gl_Position = proj_view * model * vec4(aPos, 1.0);

    Center = vec3(proj_view * model * vec4(0,0,0,1.0));
    FragPos = vec3(gl_Position);

    TexCoords = aTexCoords;
}