layout (location = 0) out vec4 FragColor;
  
layout (location = 0) in vec2 TexCoords;
layout (location = 1) in vec3 Normal;
layout (location = 2) in vec3 FragPos;
layout (location = 3) in mat3 TBN;

#ifndef IS_DEPTH_ONLY
layout (set = 1, binding = 0) uniform sampler2D diffuse;
layout (set = 1, binding = 1) uniform sampler2D metallic;
layout (set = 1, binding = 2) uniform sampler2D roughness;
layout (set = 1, binding = 3) uniform sampler2D normal;
layout (set = 1, binding = 4) uniform sampler2D height;

#include shaders/pbr_helper.glsl

layout (push_constant) uniform Paralax {
	int steps;
	float depth_scale;
};

vec2 parallax_uv(vec2 texCoords, vec3 viewDir)
{
	// number of depth layers
    const float numLayers = steps;
	const float height_scale = depth_scale;
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy * height_scale; 
    vec2 deltaTexCoords = P / numLayers;

	// get initial values
	vec2  currentTexCoords     = texCoords;
	float currentDepthMapValue = (1.0 - texture(height, currentTexCoords).r);
	
	while(currentLayerDepth < currentDepthMapValue)
	{
		// shift texture coordinates along direction of P
		currentTexCoords -= deltaTexCoords;
		// get depthmap value at current texture coordinates
		currentDepthMapValue = (1.0 - texture(height, currentTexCoords).r);  
		// get depth of next layer
		currentLayerDepth += layerDepth;  
	}

	// get texture coordinates before collision (reverse operations)
	vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

	// get depth after and before collision for linear interpolation
	float afterDepth  = currentDepthMapValue - currentLayerDepth;
	float beforeDepth = (1.0 - texture(height, prevTexCoords).r) - currentLayerDepth + layerDepth;
 
	// interpolation of texture coordinates
	float weight = afterDepth / (afterDepth - beforeDepth);
	vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

	return finalTexCoords;  
}

#endif

void main()
{
#ifndef IS_DEPTH_ONLY

    vec3 viewDir = normalize(view_pos - FragPos);

    vec2 tex_coords = parallax_uv(TexCoords, transpose(TBN) * viewDir);

    // properties
    vec3 norm = texture(normal, tex_coords).rgb;
	norm = norm * 2.0 - 1.0;
    norm = normalize(norm);
	norm = normalize(TBN * norm);

	FragColor = pbr_frag(
		pow(texture(diffuse, tex_coords).rgb, vec3(2.2)),
		norm,
   		texture(metallic, tex_coords).r,
   		texture(roughness, tex_coords).r,
    	1.0
	);

#endif
}    