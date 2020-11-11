//
//  gold_foil.cpp
//  ChemistryProject
//
//  Created by Antonella Calvia on 29/10/2020.
//

#include "gold_foil.h"
#include "ecs/ecs.h"
#include "engine/input.h"
#include "chemistry_component_ids.h"
#include "components/transform.h"
#include "graphics/rhi/primitives.h"
#include "graphics/assets/material.h"
#include "graphics/assets/model.h"
#include "graphics/rhi/draw.h"
#include "graphics/renderer/lighting_system.h"

float frand() {
    return (float)rand() / INT_MAX * 2.0 - 1.0;
}

void update_alpha_emitter(World& world, UpdateCtx& ctx) {
    
    glm::vec3 gold_foil_pos;
    float delta_time = ctx.delta_time;
    
    for (auto [e, emitter_trans, local, emitter] : world.filter<Transform, LocalTransform, AlphaEmitter>(ctx.layermask)) {
        gold_foil_pos = emitter_trans.position - local.position;
        
        emitter.alpha_positions.allocator = &default_allocator;
        emitter.alpha_particles.allocator = &default_allocator;
        
        while (emitter.emission_cooldown <= 0 && emitter.emission_rate > 0) {
            //auto [e, trans, materials, model_renderer, alpha] = world.make<Transform, Materials, ModelRenderer, AlphaParticle>();
            
            AlphaParticle particle = {};
            particle.life = emitter.alpha_particle_life;
            particle.velocity = emitter_trans.rotation * glm::vec3(0,0,emitter.speed);
            
            emitter.alpha_positions.append(emitter_trans.position);
            emitter.alpha_particles.append(particle);
            
            emitter.emission_cooldown += 1.0 / emitter.emission_rate;
        }
        
        for (uint i = 0; i < emitter.alpha_particles.length; i++) {
            //assert(world.by_id<AlphaParticle>(e.id) == &alpha);
            
            AlphaParticle& alpha = emitter.alpha_particles[i];
            glm::vec3& position = emitter.alpha_positions[i];
            
            while (emitter.alpha_particles.length > 0) {
                if (alpha.life <= 0) {
                    position = emitter.alpha_positions.pop();
                    alpha = emitter.alpha_particles.pop();
                    continue;
                }
                
                if (glm::length(position - gold_foil_pos) < 0.001) {
                    float speed = glm::length(alpha.velocity);
                    alpha.velocity.x += frand() * 2*speed;
                    alpha.velocity.y += frand() * 2*speed;
                    alpha.velocity.z += frand() * 2*speed;
                    
                    alpha.velocity = alpha.velocity * (speed/glm::length(alpha.velocity));
                }
                
                alpha.life -= ctx.delta_time;
                position += alpha.velocity * (float)ctx.delta_time;
                
                break;
            };
        }
        
        emitter.emission_cooldown -= delta_time;
    }
    

    /*
    tvector<ID> deferred_free;
    
    for (auto [e, trans, alpha] : world.filter<Transform, AlphaParticle>(ctx.layermask)) {
        assert(world.by_id<AlphaParticle>(e.id) == &alpha);
        
        if (alpha.life <= 0) deferred_free.append(e.id);
        if (glm::length(trans.position - gold_foil_pos) < 0.001) {
            float speed = glm::length(alpha.velocity);
            alpha.velocity.x += frand() * 2*speed;
            alpha.velocity.y += frand() * 2*speed;
            alpha.velocity.z += frand() * 2*speed;
            
            alpha.velocity = alpha.velocity * (speed/glm::length(alpha.velocity));
        }
        
        alpha.life -= ctx.delta_time;
        trans.position += alpha.velocity * (float)ctx.delta_time;
    }
    
    for (ID id : deferred_free) world.free_by_id(id);*/
}

void extract_alpha_emitter_render_data(World& world, LightUBO& light_ubo, EntityQuery layermask) {
    for (auto [e, emitter_trans, emitter] : world.filter<Transform, AlphaEmitter>(layermask)) {
        for (uint i = 0; i < emitter.alpha_particles.length; i++) {
            if (light_ubo.num_point_lights == MAX_POINT_LIGHTS) break;
            float life = emitter.alpha_particles[i].life;
            if (0 <= life && life < 0.05 ) {
                PointLightUBO ubo = {};
                ubo.color = glm::vec4(0,10,0,1);
                ubo.position = emitter.alpha_positions[i];
                
                light_ubo.point_lights[light_ubo.num_point_lights++] = ubo;
            }
        }
    }
}


void render_alpha_emitter(World& world, RenderPass& render_pass, EntityQuery layermask) {
    for (auto [e, emitter_trans, emitter] : world.filter<Transform, AlphaEmitter>(layermask)) {
        
        glm::mat4* model_m = TEMPORARY_ARRAY(glm::mat4, emitter.alpha_positions.length);
        for (uint i = 0; i < emitter.alpha_positions.length; i++) {
            //glm::scale(glm::mat4(1.0), glm::vec3(0.05))
            model_m[i] = glm::translate(glm::mat4(1.0), emitter.alpha_positions[i]);
            model_m[i] = glm::scale(model_m[i], glm::vec3(0.05));
        }
        
        draw_mesh(render_pass.cmd_buffer, primitives.sphere, emitter.alpha_material, {model_m, emitter.alpha_positions.length});
    }
}
