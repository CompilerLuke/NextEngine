#include shaders/vert_helper.glsl

layout (location = 9) in vec2 displacement_offset;
layout (location = 10) in float lod;
layout (location = 11) in float edge_lod;

INTER(8) vec3 FragPosTangent;
INTER(9) vec3 viewPosTangent;
INTER(10) vec2 DisplacementTex;
INTER(11) vec3 Debug;

layout (std140, set = 2, binding = 0) uniform UBO {
	vec2 displacement_scale;
	vec2 transformUVs;
	float max_height;
    float grid_size;
} ubo;

layout (set = 2, binding = 1) uniform sampler2D displacement;


void main()
{
    TexCoords = vec2(aTexCoords.x, aTexCoords.y) * ubo.transformUVs;

    float bias = 0.001;

    bool on_edge = (aTexCoords.x < 0.001 || aTexCoords.x > 0.999) || (aTexCoords.y < 0.001 || aTexCoords.y > 0.999);

    DisplacementTex = (vec2(1.0 - aTexCoords.y, 1.0 - aTexCoords.x) * (ubo.displacement_scale + 0.0001)) + displacement_offset;

    
    float height = textureLod(displacement, DisplacementTex, on_edge ? edge_lod : lod ).x;
    //height *= ubo.max_height;

    vec2 size = textureSize(displacement, 0);
    vec2 texel = 1.0 / size;

    float R = textureLod(displacement, DisplacementTex + vec2(1,0) * texel, 0).r;
    float L = textureLod(displacement, DisplacementTex + vec2(-1,0) * texel, 0).r;
    float T = textureLod(displacement, DisplacementTex + vec2(0,1) * texel, 0).r;
    float B = textureLod(displacement, DisplacementTex + vec2(0,-1) * texel, 0).r;

    float D = 2.0;
    vec3 normal = normalize(vec3((R-L) / D, -1, (T-B) / D));
    vec3 tangent = normalize(cross(normal, vec3(0,0,1)));
    vec3 bitangent = normalize(cross(normal, tangent));


    vec3 pos = aPos*vec3(1 + bias);
    gl_Position = projection * view * model * vec4(pos + vec3(0,height,0), 1.0);

#ifndef IS_DEPTH_ONLY
    Debug = vec3(lod); //vec3(edge_lod * 0.33);
	FragPos = vec3(model * vec4(pos + vec3(0,height,0), 1.0));
	TBN = make_TBN(model, normal, tangent, bitangent); 

#endif
}