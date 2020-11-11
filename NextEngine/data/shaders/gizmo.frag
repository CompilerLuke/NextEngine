#include shaders/vert_helper.glsl


#ifndef IS_DEPTH_ONLY
layout (location = 0) out vec4 FragColor;

struct Material {
    vec3 color;
};
#endif 

void main() {
    #ifndef IS_DEPTH_ONLY 
    FragColor = vec4(color, 1);
    #endif 
}
