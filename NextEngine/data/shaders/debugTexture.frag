out vec4 FragColor;

layout(location = 0) in vec2 TexCoords;
layout(binding = 1) uniform sampler2D tex;

void main() {
    FragColor = vec4(texture(tex, TexCoords).xyz, 1);
}