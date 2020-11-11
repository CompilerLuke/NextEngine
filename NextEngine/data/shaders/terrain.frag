#include shaders/vert_helper.glsl

INTER(8) vec3 FragPosTangent;
INTER(9) vec3 viewPosTangent;
INTER(10) vec2 DisplacementTex;
INTER(11) vec3 Debug;

#ifndef IS_DEPTH_ONLY
layout (location = 0) out vec4 FragColor;
/*layout (std140, set = 2, binding = 0) uniform UBO {
	vec2 displacement_scale;
	vec2 transformUVs;
	float max_height;
} ubo; */

#define MAX_TERRAIN_TEXTURES 2

layout (set = 2, binding = 2) uniform usampler2D blend_idx_map;
layout (set = 2, binding = 3) uniform sampler2D blend_values_map;

layout (set = 2, binding = 4) uniform sampler2D diffuse_textures[MAX_TERRAIN_TEXTURES];
layout (set = 2, binding = 5) uniform sampler2D metallic_textures[MAX_TERRAIN_TEXTURES];
layout (set = 2, binding = 6) uniform sampler2D roughness_textures[MAX_TERRAIN_TEXTURES];
layout (set = 2, binding = 7) uniform sampler2D normal_textures[MAX_TERRAIN_TEXTURES];
layout (set = 2, binding = 8) uniform sampler2D height_textures[MAX_TERRAIN_TEXTURES];
//layout (set = 2, binding = 9) uniform sampler2D ao_textures[MAX_TERRAIN_TEXTURES];


#include shaders/pbr_helper.glsl

/*uniform int steps;
uniform float depth_scale;
uniform vec2 displacement_scale;

uniform Material material;

uniform vec2 transformUVs;

vec2 parallax_uv(vec2 texCoords, vec3 viewDir)
{
	// number of depth layers
    const float numLayers = steps;
	const float height_scale = (displacement_scale.x * displacement_scale.y) * depth_scale;
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy * height_scale; 
    vec2 deltaTexCoords = P / numLayers;

	// get initial values
	vec2  currentTexCoords     = texCoords;
	float currentDepthMapValue = (1.0 - texture(material.height, currentTexCoords).r);
	
	while(currentLayerDepth < currentDepthMapValue)
	{
		// shift texture coordinates along direction of P
		currentTexCoords -= deltaTexCoords;
		// get depthmap value at current texture coordinates
		currentDepthMapValue = (1.0 - texture(material.height, currentTexCoords).r);  
		// get depth of next layer
		currentLayerDepth += layerDepth;  
	}

	// get texture coordinates before collision (reverse operations)
	vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

	// get depth after and before collision for linear interpolation
	float afterDepth  = currentDepthMapValue - currentLayerDepth;
	float beforeDepth = (1.0 - texture(material.height, prevTexCoords).r) - currentLayerDepth + layerDepth;
 
	// interpolation of texture coordinates
	float weight = afterDepth / (afterDepth - beforeDepth);
	vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

	return finalTexCoords;  
}*/
#endif

#define BLEND_TAP(textures, tex_coords) (blend.r * texture(textures[idx.x], tex_coords) + blend.g * texture(textures[idx.y], tex_coords) + blend.b * texture(textures[idx.z], tex_coords))

void main()
{
	#ifndef IS_DEPTH_ONLY 

	vec3 viewDir = normalize(view_pos - FragPos);
    vec2 tex_coords = TexCoords; //parallax_uv(vec2(TexCoords.x, 1.0 - TexCoords.y),  normalize(viewPosTangent - FragPosTangent));

	uvec3 idx = texture(blend_idx_map, DisplacementTex).rgb;
	vec3 blend = texture(blend_values_map, DisplacementTex).rgb;

	vec3 diffuse = BLEND_TAP(diffuse_textures, tex_coords).rgb;
	vec3 normal = BLEND_TAP(normal_textures, tex_coords).rgb;
	float metallic = BLEND_TAP(metallic_textures, tex_coords).r;
	float roughness = BLEND_TAP(roughness_textures, tex_coords).r;
    float ao = 1.0; 
    //BLEND_TAP(ao_textures, tex_coords).r;

	//diffuse = vec3(5.0 / 255, 173.0 / 255, 44.0 / 255);

	//roughness = 0.0;

    FragColor = pbr_frag(
		pow(diffuse, vec3(2.2)),
		normal_from_texture(TBN, normal),
   		metallic,
   		roughness,
    	ao
	);

	//FragColor = vec4(Debug, 1.0);

	//normal = normal_from_texture(TBN, normal);
	//normal.y = -normal.y;

	//FragColor = vec4(normal_from_texture(TBN, normal), 1.0);

	#endif
}
