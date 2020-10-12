layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;
layout (location = 5) in mat4 model;

//todo remove model instance

layout (location = 0) out vec3 localPos;


layout (push_constant) uniform PushConstant {
    mat4 proj_view;
};

void main() {
    localPos = aPos;  
    gl_Position =  proj_view * vec4(localPos, 1.0);
}