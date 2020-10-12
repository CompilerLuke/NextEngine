layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec3 localPos;

struct Material {
    samplerCube environmentMap;
};

    //vec3 skyhorizon;
    //vec3 skytop;

//layout (location = 1) uniform samplerCube environmentMap;

void main() {
    vec3 envColor = texture(environmentMap, localPos).rgb;

    //envColor = pow(envColor, vec3(2.2));

    //vec3 pointOnSphere = normalize(localPos);
    //float a = clamp(pointOnSphere.y, 0, 1);
    //FragColor = vec4(mix(skyhorizon, skytop, a), 1.0);

    FragColor = vec4(envColor, 1.0); 
}