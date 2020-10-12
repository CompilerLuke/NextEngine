#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) out vec4 Out_Color;

layout (location = 0) in vec2 Frag_UV;
layout (location = 1) in vec4 Frag_Color;

layout (push_constant) uniform TexID {
    int tex_id;
};

layout (set = 0, binding = 1) uniform sampler2D Textures[19];

void main()
{
    Out_Color = Frag_Color * texture(Textures[tex_id], Frag_UV.st);
}