layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec2 TexCoords;
layout (location = 1) in vec3 NDC;


layout (set = 1, binding = 0) uniform CompositeUBO {
    mat4 depthProj;
};

layout (set = 1, binding = 1) uniform sampler2D depthPrepass;
layout (set = 1, binding = 2) uniform sampler2D frameMap;
layout (set = 1, binding = 3) uniform sampler2D volumetricMap;
layout (set = 1, binding = 4) uniform sampler2D cloudMap;

void main() {

    float upSampledDepth = texture(depthPrepass, TexCoords).x;

    vec4 volumeColor = vec4(0);
    vec4 cloudColor = vec4(0);
    float totalWeight = 0.0f;

    vec2 screenCoordinates = gl_FragCoord.xy;

    // Select the closest downscaled pixels.

    float xOffset = 1.0 / textureSize(volumetricMap,0).x;
    //(int(gl_FragCoord.x) % 2 == 0 ? 1.0 : 1.0) / textureSize(volumetricMap, 0).x; //int(screenCoordinates.x) % 2 == 0 ? -1 : 1;
    float yOffset =  1.0 / textureSize(volumetricMap,0).y;
    //(int(gl_FragCoord.y) % 2 == 0 ? 1.0 : 1.0) / textureSize(volumetricMap, 0).y;
    

    vec2 offsets[] = {vec2(0, 0),
    vec2(0, yOffset),
    vec2(xOffset, 0),
    vec2(xOffset, yOffset)};

    float texel = 1.0 / textureSize(volumetricMap, 0).x;



    for (int a = -2; a <= 2; a++) {
        for (int b = -2; b <= 2; b++) {
            
            vec2 offsetTexCoords = TexCoords + vec2(a,b) / textureSize(volumetricMap, 0).xy;
            float downscaledDepth = textureLod(depthPrepass, offsetTexCoords, 0).r;

            float currentWeight = 1.0f;
            currentWeight *= max(0.0f, 1.0f - (0.05f) * abs(downscaledDepth - upSampledDepth));

            volumeColor += currentWeight * texture(volumetricMap, offsetTexCoords);
            cloudColor += currentWeight * texture(cloudMap, offsetTexCoords);
            totalWeight += currentWeight;
        }
    }

    /*
    for (int i = 0; i < 4; i ++)
    {

        vec2 offsetTexCoords = TexCoords + offsets[i];
        vec3 downscaledColor = textureLod(volumetricMap, offsetTexCoords, 0).xyz;
        float downscaledDepth = textureLod(depthPrepass, offsetTexCoords, 0).r;

        float currentWeight = 1.0f;
        currentWeight *= max(0.0f, 1.0f - (0.05f) * abs(downscaledDepth - upSampledDepth));

        volumeColor += downscaledColor * currentWeight;
        totalWeight += currentWeight;
    }
    */
    
    const float epsilon = 0.0001f;

    vec4 volumetricLight = volumeColor/ (totalWeight + epsilon);
    vec4 cloudLight = cloudColor/ (totalWeight + epsilon);
    
    vec4 dist4 = depthProj * vec4(0,0, upSampledDepth, 1.0);
    dist4 /= dist4.w;

    float dist = -dist4.z; // min(-dist4.z, cloudLight.w);
    float FogDensity = 400.0f;
    //float fogFactor = 1.0 / pow(dist * 1.02, 2);
    
    float fogStart = 30;
    float fogEnd = 300;

    float fogFactor = (fogEnd - dist) / (fogEnd - fogStart);
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    float cloud_shadow = volumetricLight.w;
 
    vec3 fogColor = vec3(66, 188, 245) / 200;

    vec3 currentFrame = cloud_shadow*texture(frameMap, vec2(TexCoords.x, TexCoords.y)).xyz;
    //currentFrame += volumetricLight.xyz;
    //currentFrame *= (1.0 - volumetricLight.w);
 
    currentFrame =  fogColor*(1.0 - fogFactor) + currentFrame * fogFactor;   
    currentFrame = cloudLight.w*currentFrame + cloudLight.xyz;
    //currentFrame =  fogColor*(1.0 - fogFactor) + currentFrame * fogFactor;   

    currentFrame += volumetricLight.xyz;

    //currentFrame = cloudColor.xyz;

    currentFrame = currentFrame / (currentFrame + vec3(1.0));
    currentFrame = pow(currentFrame, vec3(1.0/2.2));

    FragColor = vec4(currentFrame, 1.0);
}
