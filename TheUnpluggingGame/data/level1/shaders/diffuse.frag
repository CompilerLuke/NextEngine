#include shaders/vert_helper.glsl

struct Material {
	channel3 diffuse;
};

#ifndef IS_DEPTH_ONLY
layout (location = 0) out vec4 FragColor;

#include shaders/pbr_helper.glsl

vec3 CalcDirLight(vec3 N, vec3 V) {
	vec3 L = normalize(-dir_light.direction);
	float diff = max(dot(N, L), 0.0);

    return diff * texture(diffuse, TexCoords).rgb;
}

vec3 CalcPointLight(int i, vec3 N, vec3 WorldPos, vec3 V)
{
    vec3 L = normalize(point_lights[i].position - WorldPos);
	float diff = max(dot(N, L), 0.0);

	return diff * texture(diffuse, TexCoords).rgb;
}
#endif

void main()
{
    #ifndef IS_DEPTH_ONLY
    // properties
    vec3 view_dir = normalize(view_pos - FragPos);

	vec3 Lo = vec3(0.0);
    
    vec3 norm = normalize(TBN * vec3(0,1,0));

    // phase 1: Directional lighting
    Lo += CalcDirLight(norm, view_dir);
    // phase 2: Point lights
    for(int i = 0; i < point_lights_count; i++) {
        Lo += CalcPointLight(i, norm, FragPos, view_dir);
	}

	vec3 ambient = texture(diffuse, TexCoords).rgb * 0.3;

    vec3 color = ambient + Lo;

    FragColor = vec4(color, 1);
    #endif
}