#include shaders/vert_helper.glsl

void main()
{
    gl_Position = projection * view * model * vec4(aPos + aNormal * 0.05, 1.0);
}
