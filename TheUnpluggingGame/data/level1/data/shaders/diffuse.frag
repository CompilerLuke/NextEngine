layout (location = 0) out vec4 FragColor;

layout(location = 0) in vec2 TexCoords;
layout(location = 1) in vec3 FragPos;
layout(location = 2) in mat3 TBN;
layout(location = 6) in vec3 norm;

#include shaders/pbr_helper.glsl

struct Material {
	channel3 diffuse;
};

vec3 CalcDirLight(DirLight light, vec3 N, vec3 V) {
	vec3 L = normalize(-light.direction);
	float diff = max(dot(N, L), 0.0);

    return diff * texture(diffuse, TexCoords).rgb;
}

vec3 CalcPointLight(PointLight light, vec3 N, vec3 WorldPos, vec3 V)
{
    vec3 L = normalize(light.position - WorldPos);
	float diff = max(dot(N, L), 0.0);

	return diff * texture(diffuse, TexCoords).rgb;
}

void main()
{
    // properties
    vec3 view_dir = normalize(view_pos - FragPos);

	vec3 Lo = vec3(0.0);

    // phase 1: Directional lighting
    Lo += CalcDirLight(dir_light, norm, view_dir);
    // phase 2: Point lights
    for(int i = 0; i < point_lights_count; i++) {
        Lo += CalcPointLight(point_lights[i], norm, FragPos, view_dir);
	}

	vec3 ambient = texture(diffuse, TexCoords).rgb * 0.3;

    vec3 color = ambient + Lo;

    FragColor = vec4(color, 1);
}