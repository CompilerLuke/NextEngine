layout (location = 0) out vec4 FragColor;
layout (location = 0) in vec2 TexCoords;

layout (set = 2, binding = 0, std140) uniform CompositeUBO {
	mat4 toDepth;
	float fogBegin;
	float fogEnd;
	vec3 fogColor;
};

layout (set = 2, binding = 1) uniform sampler2D depthMap;
layout (set = 2, binding = 2) uniform sampler2D sceneMap;
layout (set = 2, binding = 3) uniform sampler2D volumetricMap;
layout (set = 2, binding = 4) uniform sampler2D cloudMap;

void main() {
	float depth = texture(depthMap, TexCoords).r;
	vec4 depth4 = toDepth * vec4(0,0,depth,1);
	depth = depth4.z / depth4.w;
	
	vec3 sceneColor = texture(sceneMap, TexCoords).xyz;
	vec4 volumetricColor;
	vec4 cloudColor;


	float weight = 0.0;
	for (int a = -2; a <= 2; a++) {
		for (int b = -2; b <= 2; b++) {
			volumetricColor += texture(volumetricMap, TexCoords + vec2(a,b)/textureSize(volumetricMap,0).xy);
			cloudColor += texture(volumetricMap, TexCoords + vec2(a,b)/textureSize(cloudMap,0).xy);
		}
	}

	
	float fogAmount = smoothstep(fogBegin, fogEnd, depth);

	vec3 finalColor = mix(sceneColor, fogColor, fogAmount);
	finalColor = mix(finalColor, cloudColor.xyz, cloudColor.a);		
	finalColor += volumetricColor.xyz; 

	float gamma = 2.2;
	finalColor = pow(finalColor, vec3(1.0/gamma));
	
	FragColor = vec4(finalColor, 1.0);
}

