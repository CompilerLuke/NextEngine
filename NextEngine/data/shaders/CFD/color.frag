#include shaders/vert_helper.glsl

#ifndef IS_DEPTH_ONLY
layout (location = 0) out vec4 FragColor;

layout(std140, push_constant) uniform ColorBlock {
  layout(offset = 64) vec4 color;
  layout(offset = 80) vec4 plane_normal;
};

#endif 

void main() {
  if (dot(plane_normal.xyz, FragPos.xyz) < plane_normal.w) {
    discard;
  }

  vec3 normal = normal_from_texture(TBN, vec3(0.5,0.5,1.0));

  vec3 diffuse = color.rgb * max(0, dot(normal, vec3(0,1,0)));
  vec3 ambient = color.rgb;

  #ifndef IS_DEPTH_ONLY 
  FragColor = vec4(diffuse * 0.6 + 0.6*ambient, color.a);
  //vec4(color);
    
  #endif 
}
