//
//  electron.cpp
//  ChemistryProject
//
//  Created by Antonella Calvia on 04/11/2020.
//

#include "ecs/ecs.h"
#include "components/transform.h"
#include "components/lights.h"
#include "emission.h"
#include "engine/input.h"
#include "chemistry_component_ids.h"
#include <math.h>
#include "core/time.h"

float frand();

float lerp(float a, float b, float t) {
    return a * (1.0 - t) + b * t;
}

void update_electrons(World& world, UpdateCtx& ctx) {
    float dist[7] = { 5, 10, 13, 15, 12, 21.5, 21 };
    glm::vec3 colors[7] = {{255,0,0}, {0,255,0}, {0,0,255}, {100,0,255}};
    
    for (auto [e, trans, local, point, elec] : world.filter<Transform, LocalTransform, PointLight, Electron>(ctx.layermask)) {
        
        float t = 1.0;
        
        if (ctx.input.key_pressed(Key::L)) {
            elec.target_n++;
            //elec.n++;
            elec.t = t;
        }
        
        uint base = 1;
        
        float dist_from_nucleus = dist[elec.n];
        float target_dist_from_nucleus = dist[elec.target_n];
        
        float x = 0.5f * frand() * target_dist_from_nucleus;
        float angle = frand() * M_PI;
        float angle2 = frand() * M_PI;
        
        glm::quat rot = glm::angleAxis(angle, glm::vec3(0, 1, 0)) * glm::angleAxis(angle2, glm::vec3(0,0,1));
        local.position = rot * glm::vec3(x,0,0);
        
        if (elec.n != elec.target_n) {
            elec.t -= ctx.delta_time;
            
            if (elec.t <= 0) elec.n = elec.target_n;
        }
        else if (elec.n != base) {
            elec.t -= ctx.delta_time;
            
            if (elec.t <= 0) {
                elec.target_n = base;
                elec.t = 0.3;
                point.color = 100.0f * colors[elec.n - 2];
            }
        } else {
            point.color = 0.1f * glm::vec3(255,255,255);
        }
        
        
        
        //if (elec.n != elec.target_n) {
        //
        //}
        
        /*
        if (elec.n != elec.target_n) {
            float dist_from_nucleus = dist[elec.n];
            float target_dist_from_nucleus = dist[elec.target_n];
            
            local.position.x = dist_from_nucleus
            local.position.x += 5*(target_dist_from_nucleus - local.position.x) * ctx.delta_time;
            
            float dist_to_target = fabs(target_dist_from_nucleus - local.position.x);
            
            if (elec.n > elec.target_n) {
                float dist_between_shell = dist[elec.n] - dist[elec.target_n];
                point.color = glm::vec3(255,255,255) * dist_to_target;
            } else {
                point.color = glm::vec3(0);
            }
            
            if (dist_to_target < 0.01) {
                elec.n = elec.target_n;
                elec.t = 5.0;
            }
        } else if (elec.n != base) {
            elec.t -= ctx.delta_time;
            
            if (elec.t <= 0) {
                elec.target_n = base;
                elec.t = 0;
            }
        }*/
    }
}
