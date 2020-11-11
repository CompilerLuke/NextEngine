//layout (location = 0) out ivec3 Blend_idx;
layout (location = 0) out vec4 Blend;

layout (location = 0) in vec3 Center;
layout (location = 1) in vec3 FragPos; 
layout (location = 2) in vec2 TexCoords;

//layout (set = 0, binding = 0) uniform UBO { vec2 brush_size; };


/*layout (set = 0, binding = 0) uniform UBO {

}*/

layout (set = 0, binding = 1) uniform sampler2D displacement_map;
layout (set = 0, binding = 2) uniform sampler2D brush_atlas;

layout (push_constant) uniform Splat {
    vec2 brush_offset;
    float radius;
    float hardness;
    float min_height;
    float max_height;
    int material;
};

float smooth_falloff(float i, float r, float v) {
    return clamp((r - v) / (r - i), 0, 1);
}

void main() {
    vec2 brush_uv = TexCoords;
    float inner_radius = radius * hardness;
    float inner_min_height = (max_height - min_height) * (1.0 - hardness);
    float inner_max_height = max_height - inner_min_height;

    float dist = length(FragPos - Center);
    float falloff = smooth_falloff(inner_radius, radius, dist);

    vec2 displacement_uv = vec2(FragPos) * 0.5f + 0.5f;
    float height = texture(displacement_map, displacement_uv).r;

    falloff *= smooth_falloff(inner_max_height, max_height, height);

    falloff *= texture(brush_atlas, brush_uv).r;

    Blend = vec4(0,0,0,falloff);
    Blend[material] = 1.0;

        //falloff *= smooth_falloff(inner_min_height, max_height - height, dist);

    //if (height < min_height || height > max_height) falloff = 0.0f;
}

