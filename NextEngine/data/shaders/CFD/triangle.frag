layout (location = 0) in vec3 normal;
layout (location = 1) in vec4 color;

#ifndef IS_DEPTH_ONLY
layout (location = 0) out vec4 FragColor;

#endif

layout (std140, set = 0, binding = 1) uniform SimulationUBO {
    float time;
};

void main() {
  #ifndef IS_DEPTH_ONLY
//vec3 diffuse = color.rgb * max(0, dot(normal, vec3(0,1,0)));
//    vec3 ambient = color.rgb;
    
    FragColor =
    //vec4(diffuse * 0.6 + 0.6*ambient, color.a);
  vec4(color);
    
  #endif 
}
