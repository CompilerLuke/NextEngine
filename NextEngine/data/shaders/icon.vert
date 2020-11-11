#include shaders/vert_helper.glsl

void main() {
    mat4 ModelView = view * model;
    ModelView[0][0] = 1;
    ModelView[0][1] = 0;
    ModelView[0][2] = 0;

    //Column 1:
    //ModelView[1][0] = 0;
    //ModelView[1][1] = 1;
    //ModelView[1][2] = 0;

    // Column 2:
    ModelView[2][0] = 0;
    ModelView[2][1] = 0;
    ModelView[2][2] = 1;
        
    vec3 scale = vec3(model[0][0], model[1][1], model[2,2]);

    TexCoords = aTexCoords;
    vec4 pos = (view * model) * vec4(vec3(0), 1.0);
    pos += vec4(scale * aPos, 0.0);

    gl_Position = projection * pos;
}