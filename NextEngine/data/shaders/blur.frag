layout (location = 0) out vec4 FragColor;

layout (location = 1) in vec2 TexCoords;

layout (push_constant) uniform PushConstants {
    bool horizontal;
} push_constants;

layout (binding = 1) uniform  sampler2D image;
layout (binding = 2) uniform  sampler2D depth;

float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

#define TAP(tex_offset) texture(image, tex_coords + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];

void main()
{
    vec3 depth = texture(depth, TexCoords).rgb;
    vec2 tex_coords = vec2(TexCoords.x, TexCoords.y);
    
    vec2 tex_offset = 2.0 / textureSize(image, 0); // gets size of single texel
    vec3 result = texture(image, tex_coords).rgb * weight[0]; // current fragment's contribution
    
    /*
    if(horizontal)
    {
        for(int i = 1; i < 5; ++i)
        {
            result += TAP(vec2(tex_offset.x * i, 0.0));
            result += TAP(-vec2(tex_offset.x * i, 0.0));
        }
    }
    else
    {
        for(int i = 1; i < 5; ++i)
        {
            result += TAP(vec2(0.0, tex_offset.y * i));
            result += TAP(-vec2(0.0, tex_offset.y * i));
        }
    }
    */
    
    if(push_constants.horizontal)
    {
        for(int i = 1; i < 5; ++i)
        {
            result += texture(image, tex_coords + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
            result += texture(image, tex_coords - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
        }
    }
    else
    {
        for(int i = 1; i < 5; ++i)
        {
            result += texture(image, tex_coords + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
            result += texture(image, tex_coords - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
        }
    }

    FragColor = vec4(result, 1.0);
}
