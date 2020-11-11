//
//  emission.h
//  ChemistryProject
//
//  Created by Antonella Calvia on 03/11/2020.
//

#pragma once


#include "core/core.h"
#include <glm/vec3.hpp>

COMP
struct Electron {
    uint n;
    uint target_n;
    float t;
    //glm::vec3 velocity;
};

COMP
struct Zapper {
    
};

void update_electrons(struct World& world, struct UpdateCtx& ctx);




