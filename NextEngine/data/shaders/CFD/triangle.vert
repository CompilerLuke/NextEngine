layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aColor;

layout (location = 0) out vec3 Normal;
layout (location = 1) out vec4 Color;

layout (std140, set = 0, binding = 0) uniform PassUBO {
    vec2 resolution;
    mat4 projection;
    mat4 view;
};

layout (std140, set = 0, binding = 1) uniform SimulationUBO {
    float time;
};

void main() {
    gl_Position = projection * view * vec4(aPos, 1.0);
    gl_PointSize = 5;
    Normal = aNormal;
    Color = aColor;
}
