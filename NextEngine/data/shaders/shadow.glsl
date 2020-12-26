#define MAX_CASCADES 4

layout (std140, set = 1, binding = 4) uniform ShadowUBO {
    mat4 to_light_space[MAX_CASCADES];
    vec2 cascade_end[MAX_CASCADES]; //x is in clipspace, y is distance
};

layout (set = 1, binding = 5) uniform sampler2DShadow shadow_map[MAX_CASCADES];

//, 94.673
float random(vec3 seed3) {
    float dot_prduct = dot(seed3, vec3(12.9898, 78.233, 45.164));
    return fract(sin(dot_prduct) * 43758.5453);
}

vec2 poisson_disk[64] = vec2[64](
    vec2(-0.613392, 0.617481),
    vec2(0.170019, -0.040254),
    vec2(-0.299417, 0.791925),
    vec2(0.645680, 0.493210),
    vec2(-0.651784, 0.717887),
    vec2(0.421003, 0.027070),
    vec2(-0.817194, -0.271096),
    vec2(-0.705374, -0.668203),
    vec2(0.977050, -0.108615),
    vec2(0.063326, 0.142369),
    vec2(0.203528, 0.214331),
    vec2(-0.667531, 0.326090),
    vec2(-0.098422, -0.295755),
    vec2(-0.885922, 0.215369),
    vec2(0.566637, 0.605213),
    vec2(0.039766, -0.396100),
    vec2(0.751946, 0.453352),
    vec2(0.078707, -0.715323),
    vec2(-0.075838, -0.529344),
    vec2(0.724479, -0.580798),
    vec2(0.222999, -0.215125),
    vec2(-0.467574, -0.405438),
    vec2(-0.248268, -0.814753),
    vec2(0.354411, -0.887570),
    vec2(0.175817, 0.382366),
    vec2(0.487472, -0.063082),
    vec2(-0.084078, 0.898312),
    vec2(0.488876, -0.783441),
    vec2(0.470016, 0.217933),
    vec2(-0.696890, -0.549791),
    vec2(-0.149693, 0.605762),
    vec2(0.034211, 0.979980),
    vec2(0.503098, -0.308878),
    vec2(-0.016205, -0.872921),
    vec2(0.385784, -0.393902),
    vec2(-0.146886, -0.859249),
    vec2(0.643361, 0.164098),
    vec2(0.634388, -0.049471),
    vec2(-0.688894, 0.007843),
    vec2(0.464034, -0.188818),
    vec2(-0.440840, 0.137486),
    vec2(0.364483, 0.511704),
    vec2(0.034028, 0.325968),
    vec2(0.099094, -0.308023),
    vec2(0.693960, -0.366253),
    vec2(0.678884, -0.204688),
    vec2(0.001801, 0.780328),
    vec2(0.145177, -0.898984),
    vec2(0.062655, -0.611866),
    vec2(0.315226, -0.604297),
    vec2(-0.780145, 0.486251),
    vec2(-0.371868, 0.882138),
    vec2(0.200476, 0.494430),
    vec2(-0.494552, -0.711051),
    vec2(0.612476, 0.705252),
    vec2(-0.578845, -0.768792),
    vec2(-0.772454, -0.090976),
    vec2(0.504440, 0.372295),
    vec2(0.155736, 0.065157),
    vec2(0.391522, 0.849605),
    vec2(-0.620106, -0.328104),
    vec2(0.789239, -0.419965),
    vec2(-0.545396, 0.538133),
    vec2(-0.178564, -0.596057)
);

/*float ditherPattern[4][4] = {{ 0.0f, 0.5f, 0.125f, 0.625f},
{ 0.75f, 0.22f, 0.875f, 0.375f},
{ 0.1875f, 0.6875f, 0.0625f, 0.5625},
{ 0.9375f, 0.4375f, 0.8125f, 0.3125}};

float dither = ditherPattern[int(gl_FragCoord.x) % 4][int(gl_FragCoord.y) % 4];*/

/*vec3 offset_lookup(sampler2D map, vec4 loc, vec2 offset) { 
    vec2 texel_size = 1.0 / textureSize(map, 0);
    return textureProj(map, vec4(loc.xy + offset * texel_size * loc.w, loc.z, loc.w)); 
}*/

    /*if (0.0 < proj_coords.x && proj_coords.x < 1.0 && 0.0 < proj_coords.y && proj_coords.y < 1.0) {
        return 1.0;
    } else {
        return 0.0;
    }*/

    //return current_depth;

    //return float(cascade) / MAX_CASCADES;

int calc_cascade(float frag_depth) {
    int cascade;
    for(cascade = 0; cascade < MAX_CASCADES; cascade++) {
        if (frag_depth <= cascade_end[cascade].x) break;
    } 
    return cascade;
}

float calc_shadow(vec3 normal, vec3 light_dir, float frag_depth, vec3 frag_pos) {
    frag_depth = gl_FragCoord.z;
    //return (frag_depth - cascade_end[0].x) / (cascade_end[MAX_CASCADES - 1].x - cascade_end[0].x);
    
    int cascade = calc_cascade(frag_depth);
    //return float(cascade) / MAX_CASCADES;
    
    if (cascade == MAX_CASCADES) return 0.0f;

    vec4 frag_pos_light_space = to_light_space[cascade] * vec4(frag_pos, 1.0);
    frag_pos_light_space /= frag_pos_light_space.w;

    normal = normalize(normal);
    light_dir = normalize(light_dir);


    // transform to [0,1] range
    //(1.0 - float(cascade)/MAX_CASCADES) * 
    //float bias = 
    //0.007 * (1.0 - dot(normal, light_dir) * dot(normal, light_dir));

    //return dot(normal, light_dir)*dot(normal, light_dir);
    
    //max(0.003 * (1.0 - dot(normal, light_dir)), 0.005);  
    //float current_depth = proj_coords.z;


    vec3 proj_coords = frag_pos_light_space.xyz;
    proj_coords.x = proj_coords.x * 0.5 + 0.5;
    proj_coords.y = proj_coords.y * 0.5 + 0.5;

    /*if (0.0 < proj_coords.x && proj_coords.x < 1.0 && 0.0 < proj_coords.y && proj_coords.y < 1.0) {
        return 1.0;
    } else {
        return 0.0;
    }*/
    
    float slope_bias = 50;
    float current_depth = proj_coords.z;

    /*float ddistdx = dFdx(current_depth);
    float ddistdy = dFdy(current_depth);
    current_depth -= slope_bias * abs(ddistdx);
    current_depth -= slope_bias * abs(ddistdy);
    current_depth -= 0.004;*/

    float shadow = 0.0;

    vec2 texel_size = 1.0 / textureSize(shadow_map[cascade], 0);

    //vec2 rand_offset = random(frag_pos);
    int offset = int(random(frag_pos) * 64);
    int samples = 4;
    float radius = 1.0;

    //for(int i = 0; i < samples; i++) { 
    //    vec2 offset = texel_size * radius * poisson_disk[(i + offset) % 64];
   
    for (float i = -1.5; i <= 1.5; i += 1.0f) {
        for (float j = -1.5; j <= 1.5; j += 1.0f) {
            //vec2 poisson_offset = poisson_disk[(i + offset) % 64];
            vec2 offset = texel_size * vec2(i,j);
            shadow += textureProj(shadow_map[cascade], vec4(proj_coords.xy + offset, current_depth, 1.0)); 
        }
    }
    shadow /= 16;
    //return current_depth;
    //return textureProj(shadow_map[cascade], vec4(proj_coords.xy, current_depth, 1.0)); 
    return shadow;
}
