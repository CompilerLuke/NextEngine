INTER(0) vec2 TexCoords; 
INTER(1) vec3 FragPos; 
INTER(2) vec3 NDC;
INTER(3) mat3 TBN;

#ifdef VERTEX_SHADER
layout (location = 0) in vec3 aPos; 
layout (location = 1) in vec3 aNormal; 
layout (location = 2) in vec2 aTexCoords; 
layout (location = 3) in vec3 aTangent; 
layout (location = 4) in vec3 aBitangent; 
#endif

#ifdef VERTEX_SHADER
#ifdef IS_INSTANCED
layout (location = 5) in mat4 model;
#else
layout (std140, push_constant) uniform PushConstants {
	mat4 model;
};
#endif

layout (std140, set = 0, binding = 0) uniform PassUBO {
	vec2 resolution;
	mat4 projection;
	mat4 view;
};
#endif

layout (std140, set = 0, binding = 1) uniform SimulationUBO {
	float time;
};

#ifdef VERTEX_SHADER
mat3 make_TBN(mat4 model, vec3 n, vec3 t, vec3 b) {
    vec3 T = normalize(vec3(model * vec4(t, 0.0)));
    vec3 N = normalize(vec3(model * vec4(n, 0.0)));
    // re-orthogonalize T with respect to N
    T = normalize(T - dot(T, N) * N);
    // then retrieve perpendicular vector B with the cross product of T and N
    vec3 B = cross(N, T);

    if (dot(cross(N, T), B) < 0.0) T = T * -1.0;

    return mat3(T,B,N);
}
#endif

vec3 normal_from_texture(mat3 TBN, vec3 norm) {
    norm = norm * 2.0 - 1.0;
    norm = normalize(norm);
    //norm.y = -norm.y;
    return normalize(TBN * norm);
}

