#pragma once

#include "core/core.h"
#include "core/math/vec4.h"

inline vec4 rgb(float r, float g, float b, float a = 1.0) {
    return vec4(r,g,b,a) * vec4(1.0f/255.0f, 1.0);
}

//todo move to math
inline vec3 lerp(vec3 a, vec3 b, float f) {
    return a * (1.0f - f) + b * f;
}

inline vec4 lerp(vec4 a, vec4 b, float f) {
    return a * (1.0f - f) + b * f;
}

inline vec4 color_map(float value, float min_value = 0.0, float max_value = 1.0) {
    const uint n_colors = 20;
    
    static vec4 viridis[n_colors] = {
        rgb(63, 19, 80),
        rgb(66, 27, 99),
        rgb(67, 42, 115),
        rgb(67, 57, 125),
        rgb(65, 72, 132),
        rgb(68, 86, 136),
        rgb(63, 99, 137),
        rgb(63, 110, 139),
        rgb(65, 123, 140),
        rgb(68, 136, 139),
        rgb(72, 148, 139),
        rgb(78, 160, 136),
        rgb(86, 171, 130),
        rgb(99, 184, 123),
        rgb(116, 194, 113),
        rgb(138, 205, 102),
        rgb(163, 213, 90),
        rgb(191, 220, 80),
        rgb(221, 226, 78),
        rgb(249, 231, 85)
    };
    
    static vec4* colors = viridis;
    
    value = (value - min_value) / (max_value - min_value) * n_colors;
    int a = max(0, min(int(value), n_colors - 1));
    int b = min(a+1, n_colors - 1);
    return lerp(colors[a], colors[b], value - int(value));
}
