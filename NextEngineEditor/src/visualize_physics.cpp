#include "graphics/assets/assets.h"
#include "graphics/assets/material.h"
#include "graphics/rhi/primitives.h"
#include "components/transform.h"
#include "graphics/rhi/draw.h"
#include "physics/physics.h"
#include "ecs/ecs.h"
#include <glm/gtc/matrix_transform.hpp>


struct PhysicsResources {
    material_handle line;
    material_handle solid;
};

void make_physics_resources(PhysicsResources& resources) {
    {
        MaterialDesc desc{ global_shaders.gizmo };
        desc.draw_state = DepthFunc_Always | PolyMode_Wireframe | (5 << WireframeLineWidth_Offset);
        mat_vec3(desc, "color", glm::vec3(1,0,1));
        
        resources.line = make_Material(desc);
    }
    
    {
        MaterialDesc desc{ global_shaders.gizmo };
        desc.draw_state = PolyMode_Wireframe | (5 << WireframeLineWidth_Offset);
        mat_vec3(desc, "color", glm::vec3(1,0,1));
        
        resources.solid = make_Material(desc);
    }
}

void destroy_physics_resources() {
    
}

void render_colliders(PhysicsResources& resources, CommandBuffer& cmd_buffer, World& world, EntityQuery query) {
    tvector<glm::mat4> sphere_model_m;
    tvector<glm::mat4> plane_model_m;
    tvector<glm::mat4> box_model_m;
    
    for (auto [e, trans, collider] : world.filter<Transform, SphereCollider>(query)) {
        Transform collider_trans;
        collider_trans.position = trans.position;
        collider_trans.scale = glm::vec3(collider.radius);
        
        sphere_model_m.append(compute_model_matrix(collider_trans));
    }
    
    for (auto [e, trans, collider] : world.filter<Transform, PlaneCollider>(query)) {
        glm::mat4 model_m = glm::lookAt(glm::vec3(0), collider.normal, glm::vec3(0,1,0));
        model_m = glm::scale(model_m, glm::vec3(1000));
        
        plane_model_m.append(model_m);
    }
    
    for (auto [e, trans, collider] : world.filter<Transform, BoxCollider>(query)) {
        Transform collider_trans = trans;
        collider_trans.scale = glm::vec3(collider.scale);
        
        box_model_m.append(compute_model_matrix(collider_trans));
    }
    
    draw_mesh(cmd_buffer, primitives.sphere, resources.line, sphere_model_m);
    draw_mesh(cmd_buffer, primitives.quad, resources.solid, plane_model_m);
    draw_mesh(cmd_buffer, primitives.cube, resources.line, box_model_m);
}
