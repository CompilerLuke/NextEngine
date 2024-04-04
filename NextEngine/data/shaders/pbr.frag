#include shaders/vert_helper.glsl

#ifndef IS_DEPTH_ONLY
layout (location = 0) out vec4 FragColor;

#include shaders/pbr_helper.glsl

//todo not a big fan of putting tiling in the fragment shader
//but it does simplify the code
//so we will do that for now
//tiling could be put in a push constant
//maybe the same material just with that variation is used a lot

struct Material {
	channel3 diffuse;
	channel1 metallic;
	channel1 roughness;
	channel1 normal;
	vec2 tiling;
};
#endif

void main() {
	#ifndef IS_DEPTH_ONLY
    // properties
	vec2 TiledTexCoords = TexCoords * tiling;
	vec3 norm = normal_from_texture(TBN, texture(normal, TiledTexCoords).rgb);

    FragColor = pbr_frag(
		pow(texture(diffuse, TiledTexCoords).rgb, vec3(2.2)),
		norm,
		texture(metallic, TiledTexCoords).r,
		texture(roughness, TiledTexCoords).r,
        0.0, 1.0
	);

	//FragColor = vec4(norm, 1.0);

	#endif 
}
