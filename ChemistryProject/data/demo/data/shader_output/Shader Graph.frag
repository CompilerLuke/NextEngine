#include shaders/vert_helper.glsl
#include shaders/pbr_helper.glsl
layout (location = 0) out vec4 FragColor;
void main()
{
if (1.000000<0.000000) discard;
#ifndef IS_DEPTH_ONLY

FragColor = vec4(pow(vec3(0.000000,0.000000,0.000000), vec3(2.2)), 1.0) + pbr_frag(pow(vec3(0.495146,0.175945,0.175945), vec3(2.2)),
calc_normals_from_tangent(vec3(0.500000,0.500000,1.000000)),
0.000000,
0.000000,
1.0f);
#endif
}
