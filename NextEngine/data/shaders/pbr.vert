#include shaders/vert_helper.glsl

void main()
{
    TexCoords = aTexCoords;

    gl_Position = projection * view * model * vec4(aPos, 1.0);

	#ifndef IS_DEPTH_ONLY

    FragPos = vec3(model * vec4(aPos, 1.0));
	
    vec3 T = normalize(vec3(model * vec4(aTangent, 0.0)));
    vec3 N = normalize(vec3(model * vec4(aNormal, 0.0)));
    vec3 B = cross(N, T);

    TBN = make_TBN(model, aNormal, aTangent, aBitangent);
    NDC = gl_Position.xyz / gl_Position.w;

	#endif
}
