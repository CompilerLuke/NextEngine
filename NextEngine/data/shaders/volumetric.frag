#include shaders/shadow.glsl

layout (location = 0) out vec4 VolumeColor;
layout (location = 1) out vec4 CloudColor;

layout (location = 0) in vec2 TexCoords;
layout (location = 1) in vec3 NDC;

#define MAX_CASCADES 4

layout (set = 1, binding = 0) uniform VolumetricUBO {
    mat4 toWorld;
    vec3 camPosition;
    vec3 sunPosition;
    vec3 sunDirection;
    vec3 sunColor;

    int fogSteps;
    int fogCloudShadowSteps;
    int cloudSteps;
    int cloudShadowSteps;

    vec3 scatterColor;
    float fogCoefficient;
    float intensity;

    float cloud_density;
    float cloud_bottom;
    float cloud_top;
    float cloudPhase;
    float lightAbsorbtionThroughCloud;
    float lightAbsorbtionTowardsSun;
    float forwardIntensity;
    float shadowDarknessThreshold;
    vec3 wind;

    float time;
};

layout (set = 1, binding = 1) uniform sampler2D depthPrepass;

#define PI 3.14159265359

// https://www.shadertoy.com/view/4dS3Wd
float mod289(float x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 mod289(vec4 x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 perm(vec4 x){return mod289(((x * 34.0) + 1.0) * x);}

float noise(vec3 p){
    vec3 a = floor(p);
    vec3 d = p - a;
    d = d * d * (3.0 - 2.0 * d);

    vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
    vec4 k1 = perm(b.xyxy);
    vec4 k2 = perm(k1.xyxy + b.zzww);

    vec4 c = k2 + a.zzzz;
    vec4 k3 = perm(c);
    vec4 k4 = perm(c + 1.0);

    vec4 o1 = fract(k3 * (1.0 / 41.0));
    vec4 o2 = fract(k4 * (1.0 / 41.0));

    vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
    vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

    return o4.y * d.y + o4.x * (1.0 - d.y);
}

//code inspired by http://www.alexandre-pestana.com/volumetric-lights/
float ComputeScattering(float lightDotView, float coef) {
    float result = 1.0f - coef * coef;
    result /= (4.0f * PI * pow(1.0f + coef * coef - (2.0f * coef) *  lightDotView, 1.5f));

    return result;
}

float remap(float value, vec2 from, vec2 to) {
	float low1 = from.x;
	float high1 = from.y;
	float low2 = to.x;
	float high2 = to.y;
	return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
}

float saturate(float value) {
    return clamp(value, 0, 1);
}

float fbm(vec3 x, int octaves, float H) {    
    float G = exp2(-H);
    float f = 1.0;
    float a = 1.0;
    float t = 0.0;
    for( int i=0; i<octaves; i++ )
    {
        t += a*noise(f*x);
        f *= 2.0;
        a *= G;
    }
    return t;
}

float CloudDensity(vec3 worldPos, int octaves) {
    worldPos += time*wind;

    float dist = length(worldPos - camPosition);

    float c = 0.01;
    float m = 1;
    float height = 100; // - 0.05*dist;
    float coverage = saturate(smoothstep(cloud_bottom, (cloud_top+cloud_bottom)/2, worldPos.y));
    coverage *= saturate(smoothstep(cloud_top, (cloud_top+cloud_bottom)/2, worldPos.y));
    if (coverage == 0) return 0.0;
    
    float density = 0; //fbm(c*worldPos, octaves, 0.8);

    
    density = noise(c * worldPos);
    //if (density < 0.1) return 0;
    for (int i = 1; i < octaves; i++) {
        float f = pow(3,i);
        density -= pow(2,-i) * noise(c * f * worldPos);
    }

    density = coverage * saturate(m*(density) - 0.2);
    return density;
}

float CloudLightMarch(vec3 worldPos, int shadow_steps, float lightAbsorbtionTowardSun, float darknessThreshold) {
    //return CloudDensity(worldPos, 4);
    float t1 = (cloud_bottom - worldPos.y) / -sunDirection.y;
    float t2 = (cloud_top - worldPos.y) / -sunDirection.y;

    if (t1 > t2) {
        float tmp = t1;
        t1 = t2;
        t2 = tmp;
    }

    t1 = t1 < 0 ? 0 : t1;
    t2 = t2 < 0 ? 0 : t2;
    
    t1 = (worldPos.y >= cloud_bottom && worldPos.y <= cloud_top) ? 0 : t1; 

    worldPos += -sunDirection * t1;

    float totalDensity = 0.0f;
    vec3 step = (t2-t1) * -sunDirection / shadow_steps;
    float step_size = length(step);

    for (int i = 0; i < shadow_steps; i++) {
        worldPos += step;
        totalDensity += CloudDensity(worldPos, 4) * step_size;
    }

    float transmittance = exp(-totalDensity * lightAbsorbtionTowardSun);
    return darknessThreshold + transmittance * (1.0-darknessThreshold);
}

vec3 getWorldPosition(float fragDepth) {
    vec4 FragPosLightSpace = toWorld * vec4(NDC.x, NDC.y, fragDepth, 1.0);
    FragPosLightSpace /= FragPosLightSpace.w;

    return FragPosLightSpace.xyz;
}

float inShadow(float fragDepth, vec3 worldSpace) {
    //return 0.0;
    int cascade;
    for(cascade = 0; cascade < MAX_CASCADES; cascade++) {
        if (fragDepth <= cascade_end[cascade].y) break;
    } 
    if (cascade == MAX_CASCADES) return 0.0f;

    //return cascade / 3.0;

    vec4 fragPosLightSpace = to_light_space[cascade] * vec4(worldSpace, 1.0);
    fragPosLightSpace /= fragPosLightSpace.w;

    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    float bias = 0.0003; //max(0.03 * (1.0 - dot(normal, dirLightDirection)), 0.003);
    float currentDepth = projCoords.z - bias;

    return textureProj(shadow_map[cascade], vec4(projCoords.xy, currentDepth,  1.0));
}

/*float cloudBoxIntersection() {
    float t1 = (cloud_bottom - currentPosition.y) / rayDirection.y;
    float t2 = (cloud_top - currentPosition.y) / rayDirection.y;

    if (t1 > t2) {
        float tmp = t1;
        t1 = t2;
        t2 = tmp;
    }

    t1 = min(rayLength, t1);
    t2 = min(rayLength, t2);

    t1 = t1 < 0 ? 0 : t1;
    t2 = t2 < 0 ? 0 : t2;
}*/

void main() {
    float fragDepth = texture(depthPrepass, TexCoords).r;

    //fragDepth = (fragDepth * 2) - 1.0;
    //fragDepth += 0.01;



    vec3 endRayPosition = getWorldPosition(fragDepth);

    vec3 startPosition = camPosition;

    vec3 rayVector = endRayPosition.xyz - startPosition;

    //FragColor = vec4(vec3(NDC.y),1.0);

    float rayLength = length(rayVector);
    vec3 rayDirection = rayVector / rayLength;

    int steps = 10; //int(clamp(10*rayLength / 200.0, 1, 10));

    float stepLength = rayLength / steps;
    //float stepDistance = (fragDepth + 1.0) / steps;

    vec3 step = rayDirection * stepLength;

    float ditherPattern[4][4] = {{ 0.0f, 0.5f, 0.125f, 0.625f},
    { 0.75f, 0.22f, 0.875f, 0.375f},
    { 0.1875f, 0.6875f, 0.0625f, 0.5625},
    { 0.9375f, 0.4375f, 0.8125f, 0.3125}};

    float ditherValue = ditherPattern[int(gl_FragCoord.x) % 4][int(gl_FragCoord.y) % 4];

    // Offset the start position.
    startPosition += step * ditherValue;

    vec3 currentPosition = startPosition;
    float currentDepth = 0.0f;

    //FragColor = vec4(vec3(inShadow(rayLength, endRayPosition)), 1.0);   
    //return;

    vec4 accumCloud = vec4(0);
    vec3 accumFog = vec3(0);
    //float density = 0.0;

    float depthToCloud = rayLength;

    float t1 = (cloud_bottom - currentPosition.y) / rayDirection.y;
    float t2 = (cloud_top - currentPosition.y) / rayDirection.y;

    if (t1 > t2) {
        float tmp = t1;
        t1 = t2;
        t2 = tmp;
    }

    float maxShadowDistance = fragDepth == 1 ? 1000 : rayLength;

    int cloud_steps = int(clamp(cloudSteps*rayLength / 400.0, 1.0, cloudSteps));
    if (t1 > maxShadowDistance) cloud_steps = 0;

    t1 = min(maxShadowDistance, t1);
    t2 = min(maxShadowDistance, t2);

    t1 = t1 < 0 ? 0 : t1;
    t2 = t2 < 0 ? 0 : t2;

    t1 = (startPosition.y >= cloud_bottom && startPosition.y <= cloud_top) ? 0 : t1; 

    currentPosition += t1 * rayDirection;


    vec3 cloud_step = rayDirection*(t2-t1) / cloud_steps;
    float cloud_step_size = length(cloud_step);

    float NDotView = dot(-rayDirection, sunDirection);

    float transmittance = 1.0;
    vec3 lightEnergy = vec3(0);

    float forward = forwardIntensity * ComputeScattering(NDotView, 0.95);

    float totalDensity = 0.0;
    for (int i = 0; i < cloud_steps; i++) {
        float density = CloudDensity(currentPosition, 4);
        totalDensity += density * cloud_step_size;

        if (density > 0) {
            float lightTransmittance = CloudLightMarch(currentPosition, cloudShadowSteps, 0.5, 0.1);
            lightEnergy += density * cloud_step_size * transmittance * lightTransmittance * (cloudPhase + forward);
            //lightEnergy += density * cloud_step_size * lightTransmittance * forward * phaseVal;
            transmittance *= exp(-density * cloud_step_size * lightAbsorbtionThroughCloud);


            if (transmittance < 0.01) {
                break;
            }
        }

        currentPosition += cloud_step;
    }

    accumCloud.xyz = lightEnergy * vec3(1.0);
    accumCloud.w = transmittance;

    //accumCloud *= (1.0 - CloudShadow(currentPosition, 200, 10));

    currentPosition = startPosition;
    currentDepth = 0.0;

    for (int i = 0; i < steps; i++) {
        float shadowed = inShadow(currentDepth, currentPosition);
        
        float light_transmittance = CloudLightMarch(currentPosition, fogCloudShadowSteps, 1.0, 0.1);

        //accumFog += scatterColor * shadowed;  
        accumFog += stepLength * light_transmittance * (1.0 - shadowed) * intensity/10.0 * scatterColor * ComputeScattering(NDotView, fogCoefficient);

        currentPosition += step;
        currentDepth += stepLength;
    }
    //accumFog /= steps;
    //accumFog = (currentDepth/400)

    float cloud_shadow = CloudLightMarch(endRayPosition, 1, 2.0, 0.5);
    cloud_shadow = fragDepth == 1 ? 0 : cloud_shadow;

    VolumeColor = vec4(accumFog,cloud_shadow);
    CloudColor = vec4(vec3(accumCloud.xyz), 1.0);
}

