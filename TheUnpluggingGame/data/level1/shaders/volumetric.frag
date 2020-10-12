layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec2 TexCoords;
layout (location = 1) in vec3 NDC;

layout (binding = 0) uniform UBO {
    mat4 toWorld;
    mat4 toLight;
    vec3 camPosition;
    int cascadeLevel;
    float endCascade;
    vec3 sunColor;
    vec3 sunPosition;
    vec3 sunDirection;
    float gCascadeEndClipSpace[2];
};

layout (binding = 1) uniform sampler2D depthPrepass;
layout (binding = 2) uniform sampler2D depthMap;

#define G_SCATTERING 0.8
#define NB_STEPS 20
#define NB_MAX_STEPS 25
#define PI 3.14159265359
#define scatterColor sunColor * 0.8


//code inspired by http://www.alexandre-pestana.com/volumetric-lights/
float ComputeScattering(float lightDotView) {
    float result = 1.0f - G_SCATTERING * G_SCATTERING;
    result /= (4.0f * PI * pow(1.0f + G_SCATTERING * G_SCATTERING - (2.0f * G_SCATTERING) *  lightDotView, 1.5f));

    return result;
}

vec3 getWorldPosition(float fragDepth) {
    vec4 FragPosLightSpace = toWorld * vec4(NDC.x, NDC.y, fragDepth, 1.0);
    FragPosLightSpace /= FragPosLightSpace.w;

    return FragPosLightSpace.xyz;
}

float inShadow(vec3 worldSpace) {
    vec4 fragPosLightSpace = toLight * vec4(worldSpace, 1.0);

    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;

    //if (projCoords.x > 1 || projCoords.x < 0 || projCoords.y > 1 || projCoords.y < 0) return 0.0f;

    float bias = 0.0003; //max(0.03 * (1.0 - dot(normal, dirLightDirection)), 0.003);
    float currentDepth = projCoords.z;

    float pcfDepth = texture(depthMap, projCoords.xy ).r;

    return (currentDepth - bias) > pcfDepth ? 1.0f : 0.0f;
}

void main()
{
    float fragDepth = texture(depthPrepass, TexCoords).r;

    fragDepth = (fragDepth * 2) - 1.0;
    //fragDepth += 0.01;

    float currentDepth = -1.0f;

    vec3 endRayPosition = getWorldPosition(fragDepth);

    vec3 startPosition = camPosition;

    vec3 rayVector = endRayPosition.xyz - startPosition;

    float rayLength = length(rayVector);
    vec3 rayDirection = rayVector / rayLength;

    float stepLength = rayLength / NB_STEPS;
    float stepDistance = (fragDepth + 1.0) / NB_STEPS;

    vec3 step = rayDirection * stepLength;

    float ditherPattern[4][4] = {{ 0.0f, 0.5f, 0.125f, 0.625f},
    { 0.75f, 0.22f, 0.875f, 0.375f},
    { 0.1875f, 0.6875f, 0.0625f, 0.5625},
    { 0.9375f, 0.4375f, 0.8125f, 0.3125}};

    float ditherValue = ditherPattern[int(gl_FragCoord.x) % 4][int(gl_FragCoord.y) % 4];

    // Offset the start position.
    startPosition += step * ditherValue;

    vec3 currentPosition = startPosition;

    vec3 accumFog = 0.0f.xxx;

    for (int i = 0; i < NB_STEPS; i++)
    {
        if (gCascadeEndClipSpace[0] <= currentDepth && currentDepth <= gCascadeEndClipSpace[1] && currentDepth != 1.0) {
            if (inShadow(currentPosition) < 0.5) {
                accumFog += ComputeScattering(dot(-rayDirection, sunDirection)).xxx * scatterColor;
            }
        }
        currentPosition += step;
        currentDepth += stepDistance;
    }
    accumFog /= NB_STEPS;

    


    FragColor = vec4(accumFog, 0.0);
    
}

