#include shaders/vert_helper.glsl
#include shaders/pbr_helper.glsl
layout (location = 0) out vec4 FragColor;
void main()
{
if (1.<0.) discard;
#ifndef IS_DEPTH_ONLY

FragColor = vec4(pow(vec3(0.,0.,0.), vec3(2.2)), 1.0) + pbr_frag(pow(vec3(0.,0.,0.), vec3(2.2)),
calc_normals_from_tangent(vec3(0.5,0.5,1.)),
0.,
0.,
1.0f);
#endif
}
