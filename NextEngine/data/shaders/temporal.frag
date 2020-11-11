out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;
in mat3 TBN;

uniform sampler2D previous;
uniform sampler2D current;

uniform vec3 viewPos;

void main()
{
    FragColor = vec3(); //vec4(vec3(gl_FragCoord.z), 1);
}

