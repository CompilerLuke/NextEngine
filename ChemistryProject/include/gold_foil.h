//
//  gold_foil.h
//  ChemistryProject
//
//  Created by Antonella Calvia on 29/10/2020.
//

#pragma once

#include "engine/core.h"
#include "engine/handle.h"
#include "core/container/vector.h"
#include <glm/vec3.hpp>
#include "ecs/id.h"

COMP
struct AlphaParticle {
    glm::vec3 velocity = glm::vec3(0);
    float life = 10.0;
};

COMP
struct AlphaEmitter {
    //float test = 3.0;
    float speed = 10.0;
    float emission_rate = 10;
    float alpha_particle_life = 5.0;
    material_handle alpha_material;
    //material_handle alpha_exited_material;
    //bool random_field = false;
    REFL_FALSE float emission_cooldown = 0.0;
    
    REFL_FALSE vector<glm::vec3> alpha_positions;
    REFL_FALSE vector<AlphaParticle> alpha_particles;
};


void update_alpha_emitter(struct World&, struct UpdateCtx& ctx);
void extract_alpha_emitter_render_data(struct World&, struct LightUBO& ubo, EntityQuery query);
void render_alpha_emitter(struct World&, struct RenderPass&, EntityQuery query);
