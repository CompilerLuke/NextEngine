layout (location = 0) out vec4 FragColor;
layout (location = 0) in vec2 TexCoords;

layout (binding = 1) uniform sampler2D frameMap;

void main() {
    vec3 currentFrame = texture(frameMap, vec2(TexCoords.x, TexCoords.y)).xyz;

    currentFrame = currentFrame / (currentFrame + vec3(1.0));
    currentFrame = pow(currentFrame, vec3(1.0/2.2));

    FragColor = vec4(currentFrame, 1.0);
}