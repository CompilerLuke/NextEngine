#include shaders/shadow.glsl

struct DirLight {
    vec3 direction;
    vec3 color;
};

struct PointLight {
	vec3 position;
	float radius;
    vec3 color;
};

#define MAX_NR_POINT_LIGHTS 4


/*layout (std140, set = 0, binding = 0) uniform PassUBO {
	vec2 resolution;
	mat4 projection;
	mat4 view;
};*/

layout (std140, set = 1, binding = 0) uniform LightingUBO {
	DirLight dir_light;
	PointLight point_lights[MAX_NR_POINT_LIGHTS];
	vec3 view_pos;
	int point_lights_count;
};


layout (set = 1, binding = 1) uniform samplerCube irradiance_map;
layout (set = 1, binding = 2) uniform samplerCube prefilter_map;
layout (set = 1, binding = 3) uniform sampler2D   brdf_LUT;
//layout (set = 0, binding = 5) uniform sampler2D   shadow_mask_map;

const float PI = 3.14159265359;

vec3 _albedo;
vec3 _normal;
float _metallic;
float _roughness;
float _translucency;
float _ao;

vec3 fresnel_schlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnel_schlick_roughness(float cosTheta, vec3 F0, float _roughness)
{
    return F0 + (max(vec3(1.0 - _roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float distribution_GGX(vec3 N, vec3 H, float _roughness)
{
    float a      = _roughness*_roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float geometry_schlick_GGX(float NdotV, float _roughness)
{
    float r = (_roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}


float geometry_smith(vec3 N, vec3 V, vec3 L, float _roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = geometry_schlick_GGX(NdotV, _roughness);
    float ggx1  = geometry_schlick_GGX(NdotL, _roughness);

    return ggx1 * ggx2;
}

#define G_SCATTERING 0.9

float compute_scattering(float light_dot_view) {
    float result = 1.0f - G_SCATTERING * G_SCATTERING;
    result /= (4.0f * PI * pow(1.0f + G_SCATTERING * G_SCATTERING - (2.0f * G_SCATTERING) *  light_dot_view, 1.5f));

    return result;
}

vec3 calc_dir_light(vec3 N, vec3 V) {
	vec3 L = normalize(-dir_light.direction);
	vec3 H = normalize(V + L);

    vec3 radiance     = dir_light.color;

	float wrap = 0.5;

	vec3 F0 = vec3(0.04);
	F0      = mix(F0, _albedo, _metallic);
	vec3 F  = fresnel_schlick(max((dot(H, V) + wrap) / (1 + wrap), 0.0), F0);

	float NDF = distribution_GGX(N, H, _roughness);
	float G   = geometry_smith(N, V, L, _roughness);

	float NdotL = max(dot(L,N), 0.0);

	// float diffuse = max(0, dot(L, N)); float wrap_diffuse = max(0, (dot(L, N) + wrap) / (1 + wrap)); 

// max(dot(N, L), 0.0)

	//cook torrance BRDF
	vec3 numerator    = NDF * G * F;
	float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
	vec3 specular     = numerator / max(denominator, 0.001);

	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;

	kD *= 1.0 - _metallic;

	const float PI = 3.14159265359;

	//float forward_scattering = ComputeScattering(dot(N, L)) * 0.2;

    return (kD * _albedo / PI + specular) * radiance * NdotL;
}

vec3 calc_point_light(int index, vec3 N, vec3 WorldPos, vec3 V)
{
	vec3 delta = point_lights[index].position - WorldPos;

    vec3 L = normalize(delta);
	vec3 H = normalize(V + L);

	//todo use dot product trick
	float distance    = length(delta);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance     = point_lights[index].color * attenuation;

	vec3 F0 = vec3(0.04);
	F0      = mix(F0, _albedo, _metallic);
	vec3 F  = fresnel_schlick(max(dot(H, V), 0.0), F0);

	float NDF = distribution_GGX(N, H, _roughness);
	float G   = geometry_smith(N, V, L, _roughness);

	//cook torrance BRDF
	vec3 numerator    = NDF * G * F;
	float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
	vec3 specular     = numerator / max(denominator, 0.001);

	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;

	kD *= 1.0 - _metallic;

	const float PI = 3.14159265359;

    float NdotL = max(dot(N, L), 0.0);
    return (kD * _albedo / PI + specular) * radiance * NdotL;
}

vec3 calc_normals_from_tangent(vec3 norm) {
	norm = norm * 2.0 - 1.0;
	norm = normalize(norm);
	norm = normalize(TBN * norm);

	return norm;
}


vec2 calc_tex_coords(vec2 scale, vec2 offset) {
	return (TexCoords * scale) + offset;
}

float remap(float value, vec2 from, vec2 to) {
	float low1 = from.x;
	float high1 = from.y;
	float low2 = to.x;
	float high2 = to.y;
	return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
}

// https://www.shadertoy.com/view/4dS3Wd
float hash(float n) { return fract(sin(n) * 1e4); }
float hash(vec2 p) { return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }

float noise(vec2 x) {
    vec2 i = floor(x);
    vec2 f = fract(x);

	// Four corners in 2D of a tile
	float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    // Simple 2D lerp using smoothstep envelope between the values.
	// return vec3(mix(mix(a, b, smoothstep(0.0, 1.0, f.x)),
	//			mix(c, d, smoothstep(0.0, 1.0, f.x)),
	//			smoothstep(0.0, 1.0, f.y)));

	// Same code, with the clamps in smoothstep and common subexpressions
	// optimized away.
    vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}


vec4 pbr_frag(vec3 mat_albedo, vec3 mat_normal, float mat_metallic, float mat_roughness, float mat_translucency, float mat_ao) {
    vec3 viewDir = normalize(view_pos - FragPos);
    
    _albedo     = mat_albedo;
    _normal = mat_normal;
    _metallic = mat_metallic;
    _roughness = mat_roughness;
    _ao = mat_ao;
	_translucency = mat_translucency;

    vec3 Lo = vec3(0.0);
	vec3 WorldPos = FragPos;
	vec3 V = viewDir;
	vec3 N = _normal;

	vec3 F0 = vec3(0.04);
    F0 = mix(F0, _albedo, _metallic);

    // phase 1: Directional lighting
    float shadow = calc_shadow(_normal, dir_light.direction, NDC.z, FragPos); //texture(shadow_mask_map, vec2(gl_FragCoord) / resolution).r;
    //shadow = 0.0f;

    Lo += (1.0 - shadow) * calc_dir_light(_normal, V);
	Lo += _translucency * dir_light.color * _albedo * compute_scattering(max(dot(V, dir_light.direction), 0.0));

    // phase 2: Point lights
    for(int i = 0; i < point_lights_count; i++) {
        Lo += calc_point_light(i, _normal, FragPos, V);
	}

	vec3 R = reflect(-V, N);

    vec3 F = fresnel_schlick_roughness(max(dot(N, V), 0.0), F0, _roughness);

	vec3 kS = F;
	vec3 kD = 1.0 - kS;
	kD *= 1.0 - _metallic;

	vec3 irradiance = texture(irradiance_map, N).rgb;
    
	vec3 diffuse    = irradiance * _albedo;


	const float MAX_REFLECTION_LOD = 7.0;
	vec3 prefilteredColor = textureLod(prefilter_map, R, MAX_REFLECTION_LOD * _roughness).rgb;
	vec2 envBRDF  = texture(brdf_LUT, vec2(max(dot(N, V), 0.0), _roughness)).rg;
	vec3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

	vec3 ambient = (kD * diffuse + specular) * (1.0 - shadow * 0.6);


	vec3 color = ambient + Lo;

	//color = color / (color + vec3(1.0));
	//color = pow(color, vec3(1.0/2.2));

	//return vec4(vec3(shadow), 1.0);
    return vec4(color, 1.0);
}
