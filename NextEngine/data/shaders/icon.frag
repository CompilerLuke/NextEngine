#include shaders/vert_helper.glsl

#ifndef IS_DEPTH_ONLY
layout (location = 0) out vec4 FragColor;

struct Material {
    channel4 icon;
};
#endif 


void main() {
    #ifndef IS_DEPTH_ONLY 
    vec4 color = texture(icon, TexCoords);
    if (color.a < 0.5) { discard; }

    FragColor = vec4(color.rgb, 1.0);
    
    #endif 
}