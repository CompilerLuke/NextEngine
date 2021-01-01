#include shaders/vert_helper.glsl

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TBN = make_TBN(model, aNormal, aTangent, aBitangent);
}