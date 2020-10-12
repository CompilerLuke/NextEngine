layout (location = 0) out vec4 FragColor;

#define MAX_MATRIX_N 12

layout (std140, set = 0, binding = 0) uniform KrigingUBO {
    float a; // (sill - nugget)
    float b; // (-3.0f / range)
    int N;

    vec4 positions[MAX_MATRIX_N];
    vec4 c1[MAX_MATRIX_N * MAX_MATRIX_N / 4];
};

float cov(float h) {
    return a * exp(b*h);
}

void main() {
    int N_POINTS = N - 1;
    vec2 pos = vec2(gl_FragCoord);

    float c0[MAX_MATRIX_N];
    c0[N - 1] = 1;

    for (int i = 0; i < N_POINTS; i++) {
        c0[i] = cov(length(positions[i].xy - pos));
    }

    float height = 0.0f;

    for (int i = 0; i < N_POINTS; i++) {
        float weight = 0.0f;

        for (int j = 0; j < N; j++) {
            int index = i*N + j;
            weight += c1[index / 4][index % 4] * c0[j];
        }

        height += weight * positions[i].z;
    }
    
    FragColor = vec4(height, 0, 0, 1);
}

