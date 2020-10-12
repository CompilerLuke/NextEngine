#include shaders/vert_helper.glsl

#ifndef DEPTH_ONLY
layout(location = 0) out vec4 outColor;
#endif

struct Material {
    channel3 diffuse;
    channel1 metallic;
    channel1 roughness;
    channel1 normal;
};


void main() {
    #ifndef DEPTH_ONLY
    vec3 norm = normal_from_texture(TBN, texture(normal, TexCoords).rgb);
    vec3 light = normalize(vec3(-1,1,0));
    vec3 albedo = texture(diffuse, TexCoords).rgb;
    vec3 diffuse = albedo * dot(light, norm);
    vec3 ambient = albedo * 0.2f;
    
    outColor = vec4(diffuse + albedo, 1.0);
    //outColor = vec4(0.5, 0.5, 1.0, 1.0);
    #endif
}

