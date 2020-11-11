#ifndef IS_DEPTH_ONLY
layout (location = 0) out vec4 FragColor;
#endif 

#include shaders/vert_helper.glsl

struct Material {
	channel3 diffuse;
	channel1 metallic;
	channel1 roughness;
	channel1 normal;
	float cutoff;
};

#include shaders/pbr_helper.glsl

#ifndef IS_DEPTH_ONLY

float wrap_max(float d, float wrap) {
    return max(0, (d + wrap) / (1 + wrap));
}

#endif

void main()
{
	vec3 albedo     = texture(diffuse, TexCoords).rgb;
	vec3 true_color = albedo;

	if (cutoff > 0) {
		if ((true_color.r + true_color.g + true_color.b) / 3.0f < cutoff) {
			discard;
		}
	}
	else {
		if ((true_color.r + true_color.g + true_color.b) / 3.0f > -cutoff) {
			discard;
		}
	}

	#ifndef IS_DEPTH_ONLY



	albedo = pow(albedo, vec3(2.2));

    // properties
    vec3 norm = texture(normal, TexCoords).rgb;
	norm = normalize(norm * 2.0 - 1.0);

	if (!gl_FrontFacing) norm.y = -norm.y; 
	norm = normalize(TBN * norm);

	//vec3(0,1,0);

    FragColor = pbr_frag(
		albedo,
		norm,
		texture(metallic, TexCoords).r,
		texture(roughness, TexCoords).r,
		1.0
	);

	#endif
}