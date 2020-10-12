out vec4 FragColor;
in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;
in mat3 TBN;

#include shaders/pbr_helper.glsl

void main()
{
if (1.000000<0.000000) discard;
#ifndef IS_DEPTH_ONLY

FragColor = vec4(pow(vec3(0.000000,0.000000,0.000000), vec3(2.2)), 1.0) + pbr_frag(pow(vec3(0.000000,0.000000,0.000000), vec3(2.2)),
calc_normals_from_tangent(vec3(0.500000,0.500000,1.000000)),
0.000000,
0.000000,
1.0f);
#endif
}
