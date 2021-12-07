//
//  visual.cpp
//  CFD
//
//  Created by Antonella Calvia on 12/09/2021.
//

#include <stdio.h>
#include "ui/ui.h"
#include "visualization/debug_renderer.h"
#include "core/container/vector.h"
#include "visualization/color_map.h"

struct Cubic {
    real a;
    real b;
    real c;
    real d;
    
    real operator()(real t) {
        real t2 = t*t;
        real t3 = t2*t;
        return a*t3 + b*t2 + c*t + d;
    }
};

struct Curve {
    Cubic x;
    Cubic y;
    
    vec3 operator()(real t) {
        vec3 result;
        result.x = x(t);
        result.y = y(t);
        result.z = t;
        return result;
    }
};

struct PiecewiseCurve {
    static const uint N = 3;
    
    Curve curves[N];
    real begin[N+1];
    
    vec3 at_t(real t) {
        for (uint i = 0; i < N; i++) {
            if (t <= begin[i+1]) {
                return curves[i](t);
            }
        }
        return vec3(0,0,t);
    }
    
    vec3 at_t_norm(real norm_t) {
        real length = begin[N] - begin[0];
        real t = norm_t*length + begin[0];
        return at_t(t);
    }
};

struct MathIA {
    UI* ui;
    CFDDebugRenderer* debug;
    
    PiecewiseCurve p[6];
    real L;
};

MathIA* make_math_ia(UI* ui, CFDDebugRenderer* debug) {
    MathIA* ia = PERMANENT_ALLOC(MathIA);
    ia->ui = ui;
    ia->debug = debug;
    
    return ia;
}

void set_parameters(MathIA& ia) {
    real L = 7.083;
    ia.L = L;
    
    Curve start = {{}, {-1245.52172318, 199.58259764, -9.76495080744, 0.223374209928}};
    Curve end = {{}, {0,0,1.11614,-7.65399}};
    
    ia.p[0] = {
        {start,
        {{0,0,0,0},
            {-0.000580587360269, 0.00562302065846, -0.00669429046744, 0.221}},
            end},
        {0,0,L,L}
    };
    ia.p[1] = {
        {start,
        {{0.000704768953063, -0.0272657654109, 0.15771408866,0},
            {0, 0.00449503610421, -0.0278201361325, 0.221}},
            end},
        {0,0,L,L},
    };
    ia.p[2] = {
        {start,
        {{0.000672131721692, -0.0240517267172, 0.135913932472, -0.000950219223237}, {-0.000254641285476, 0.0100557093435, -0.0558458538664, 0.171390428335}},
            end},
        {0,0.007,7.024,L},
    };
    ia.p[3] = {
        {start,
        {{-0.000415894146955, -0.00727606720638, 0.071149910902, -0.00106561014477}, {0, 0.00825756377935, -0.0543469646349, 0.109134318534}},
            end},
        {0.007, 0.015, 6.976, 7.024},
    };
    ia.p[4] = {
        {start,
        {{0,0,0,0}, {-0.000256889446751, 0.00909388708548, -0.0502040924803, 0.0779773766365}
        },
            end},
        {0.015, 0.0316, 6.93, 6.976}
    };
}

vec3 flip(-1,1,1);

void draw_shaded_quad(CFDDebugRenderer& debug, vec3 p0, vec3 p1, vec3 p2, vec3 p3, vec4 albedo) {
    vec3 p[4] = { p0,p1,p2,p3 };

    real base = 1.0;
    vec3 light = normalize(vec3(-1, -1, 0));
    vec3 normal = quad_normal(p);
    vec4 color = (1.0-base)*albedo * fmaxf(dot(normal, light),0) + albedo*base;
    color.w = 1.0;
    draw_quad(debug, p, color);
}

void draw_prism(CFDDebugRenderer& debug, vec3 p0, vec3 p1, vec3 p2, vec3 p3, real depth, vec4 color, bool start) {
    vec3 d(0,0,depth*0.99);
    
    real width = fmaxf(fmaxf(p0.x, p1.x), fmaxf(p2.x, p3.x));
    if (width == 0) return;

    draw_shaded_quad(debug, p0, p1, p2, p3, color);
    draw_shaded_quad(debug, p0 + d, p1 + d,  p1, p0, color);
    draw_shaded_quad(debug, p2, p3, p3 + d, p2 + d, color);
    draw_shaded_quad(debug, p1, p2, p2 + d, p1 + d, color);
    draw_shaded_quad(debug, p0 + d, p1 + d, p2 + d, p3 + d, color);
}

void draw_trap_prism(CFDDebugRenderer& debug, vec3 p0, vec3 p1, real depth, vec4 color, bool start) {
    draw_prism(debug, p0, p1, p1*flip, p0*flip, depth, color, start);
}

void draw_grid(CFDDebugRenderer& debug, real ax1, real ax2, vec3 offset) {
    real dim = 20;
    real dt = 0.5;
    int n = dim/dt;
    
    vec3 u = {};
    vec3 v = {};
    
    u[ax1] = 1;
    v[ax2] = 1;
    
    vec3 c = offset * dim;
    
    for (int i = -n; i <= n; i++) {
        vec4 color = i==0 ? RED_DEBUG_COLOR : vec4(0,0,0,1);
        
        draw_line(debug, u*dim + v*i*dt + c, -u*dim + v*i*dt + c, color);
        draw_line(debug, v*dim + u*i*dt + c, -v*dim + u*i*dt + c, color);
    }
}

void render_math_ia(MathIA& ia) {
    CFDDebugRenderer& debug = *ia.debug;
    clear_debug_stack(debug);
    
    set_parameters(ia);
    
#if 1
    real n = 100;
    real dt = ia.L / n;
    vec3 explode = vec3(1,1,1.0);
    
    //draw_grid(debug, 0, 2, vec3(0,0,0.5));
    draw_grid(debug, 1, 2, vec4(0,0,0,1));
    
    uint start = 0;
    for (uint i = start; i < n; i++) {
        real t = i*dt;
        
        vec3 p[6];
        for (uint j = 0; j < 5; j++) {
            p[j] = ia.p[j].at_t(t) * explode;
        }
        
        vec4 white = vec4(1);
        bool is_start = i == start;
        draw_trap_prism(debug, p[0], p[1], dt, color_map(1, 0, 4), is_start);
        draw_trap_prism(debug, p[1], p[2], dt, color_map(2, 0, 4), is_start);
        draw_trap_prism(debug, p[2], p[3], dt, color_map(3, 0, 4), is_start);
        draw_trap_prism(debug, p[3], p[4], dt, color_map(4, 0, 4), is_start);
    }
#endif
    
#if 0
    real n = 100;
    real dt = 1.0 / n;
    
    draw_grid(debug, 0, 2, vec3(0,0,0));
    vec3 scale = 1.0;
    
    for (uint i = 0; i < n; i++) {
        real t = i*dt;
        
        for (uint j = 0; j < 4; j++) {
            vec3 p[4] = {
                ia.p[j].at_t_norm(t),
                ia.p[j+1].at_t_norm(t),
                ia.p[j+1].at_t_norm(t + dt),
                ia.p[j].at_t_norm(t + dt)
            };
            
            vec3 p_f[4];
            for (uint i = 0; i < 4; i++) {
                p_f[i] = p[i] * vec3(-1,1,1);
            }
            
            vec3 normal = quad_normal(p);
            vec4 color = vec4(1,0,0,1); //color_map(j, 0, 5);
            
            draw_quad(debug, p, color);
            draw_quad(debug, p_f, color);
        }
    }
#endif
}

void destroy_math_ia(MathIA* ia) {
    
}
