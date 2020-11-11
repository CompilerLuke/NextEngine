#include shaders/vert_helper.glsl

#ifndef IS_DEPTH_ONLY
layout (location = 0) out vec4 FragColor;
#endif

void main() {
#ifndef IS_DEPTH_ONLY
    vec3 color1 = vec3(255.0, 89.0, 0) / 255.0;
    vec3 color2 = vec3(255, 162, 0) / 255.0;
    
    vec3 result = mix(color1, color2, sin(time * 3.0) * 0.5 + 0.5);
    FragColor = vec4(result, 1);
#endif
}

