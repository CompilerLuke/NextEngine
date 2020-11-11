layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec2 TexCoords;
layout (location = 1) in vec3 NDC;

layout (binding = 1) uniform sampler2D depthPrepass;
layout (binding = 2) uniform sampler2D volumetricMap;
layout (binding = 3) uniform sampler2D frameMap;
layout (push_constant) uniform Depth {
    mat4 depthProj;
};

void main()
{

    float upSampledDepth = texture(depthPrepass, TexCoords).x;

    vec3 color = 0.0f.xxx;
    float totalWeight = 0.0f;

    vec2 screenCoordinates = gl_FragCoord.xy;

    // Select the closest downscaled pixels.

    float xOffset = (int(gl_FragCoord.x) % 2 == 0 ? -1.0 : 1.0) / textureSize(volumetricMap, 0).x; //int(screenCoordinates.x) % 2 == 0 ? -1 : 1;
    float yOffset = (int(gl_FragCoord.y) % 2 == 0 ? -1.0 : 1.0) / textureSize(volumetricMap, 0).y;
    

    vec2 offsets[] = {vec2(0, 0),
    vec2(0, yOffset),
    vec2(xOffset, 0),
    vec2(xOffset, yOffset)};

    float texel = 1.0 / textureSize(volumetricMap, 0).x;



    for (float a = -8; a <= 8; a += 1.0) {
        for (float b = -8; b <= 8; b += 1.0) {
            vec2 offsetTexCoords = TexCoords + vec2(a,b) * texel;

            color += texture(volumetricMap, offsetTexCoords).xyz;
            totalWeight += 1.0;
        }
    }

    color /= 4;


    /*
    for (int i = 0; i < 4; i ++)
    {

        vec2 offsetTexCoords = TexCoords + offsets[i];
        vec3 downscaledColor = textureLod(volumetricMap, offsetTexCoords, 0).xyz;
        float downscaledDepth = textureLod(depthPrepass, offsetTexCoords, 0).r;

        float currentWeight = 1.0f;
        currentWeight *= max(0.0f, 1.0f - (0.05f) * abs(downscaledDepth - upSampledDepth));

        color += downscaledColor * currentWeight;
        totalWeight += currentWeight;
    }
    */
    
    vec3 volumetricLight;
    const float epsilon = 0.0001f;
    volumetricLight.xyz = color/ (totalWeight + epsilon);
    
    vec4 dist4 = depthProj * vec4(0,0, upSampledDepth * 2.0 - 1.0, 1.0);
    dist4 /= dist4.w;

    float dist = -dist4.z;
    float FogDensity = 400.0f;
    //float fogFactor = 1.0 / pow(dist * 1.02, 2);
    
    float fogStart = 30;
    float fogEnd = 600;

    float fogFactor = (fogEnd - dist) / (fogEnd - fogStart);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
 
    vec3 fogColor = vec3(66, 188, 245) / 200;
    
    vec3 currentFrame = texture(frameMap, vec2(TexCoords.x, TexCoords.y)).xyz;

    currentFrame = fogColor*(1.0 - fogFactor) + currentFrame * fogFactor;

    vec3 f = volumetricLight;

    currentFrame += volumetricLight;

    currentFrame = currentFrame / (currentFrame + vec3(1.0));
    currentFrame = pow(currentFrame, vec3(1.0/2.2));

    FragColor = vec4(currentFrame, 1.0);
	
}
