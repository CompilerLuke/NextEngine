layout (location = 0) in vec4 color;

#ifndef IS_DEPTH_ONLY
layout (location = 0) out vec4 FragColor;
#endif

layout (std140, set = 0, binding = 1) uniform SimulationUBO {
    float time;
};

void main() {
  #ifndef IS_DEPTH_ONLY 
  FragColor = vec4(color);
  #endif 
}
