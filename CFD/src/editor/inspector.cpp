#include "editor/inspector.h"
#include "editor/selection.h"
#include "editor/lister.h"
#include "ui/ui.h"
#include "components/transform.h"
#include "components/camera.h"
#include "cfd_components.h"
#include "cfd_ids.h"
#include "ecs/ecs.h"
#include "graphics/assets/assets.h"
#include "graphics/assets/model.h"


struct Inspector {
    World& world;
    SceneSelection& selection;
    UI& ui;
    Lister& lister;
};

Inspector* make_inspector(World& world, SceneSelection& selection, UI& ui, Lister& lister) {
    Inspector* inspector = PERMANENT_ALLOC(Inspector, { world,selection,ui, lister });

    return inspector;
}

void destroy_inspector(Inspector* inspector) {

}

template<typename T>
void field(UI& ui, string_view field, T* value, float min = -FLT_MAX, float max = FLT_MAX, float inc_per_pixel = 5.0) {
    begin_hstack(ui);
    text(ui, field).flex_h().align(HLeading); //.color(color4(220,220,220));
    input(ui, value, min, max, inc_per_pixel);
    end_hstack(ui);
}

void field(UI& ui, string_view field, input_model_handle* handle) {
    //Model* model = get_Model(*handle);

    begin_hstack(ui);
    text(ui, field);
    spacer(ui);
    text(ui, "airfoil.fbx");
    end_hstack(ui);
}

void begin_component(UI& ui, const char* title) {
    begin_vstack(ui).background(shade2);

    begin_hstack(ui);
    title2(ui, title);
    spacer(ui);
    text(ui, "X");
    end_hstack(ui);
}

void end_component(UI& ui) {
    end_vstack(ui);
    divider(ui, shade1);
}

void render_transform_component(UI& ui, Transform& trans) {
    begin_component(ui, "Transform");
    {
        field(ui, "position", &trans.position);
        field(ui, "scale", &trans.scale);
    }
    end_component(ui);
}

void render_camera_component(UI& ui, Camera& camera) {
    begin_component(ui, "Camera");
    {
        field(ui, "fov", &camera.fov, 0, 180);
        field(ui, "near", &camera.near_plane);
        field(ui, "far", &camera.far_plane);
    }
    end_component(ui);
}

void render_mesh_component(UI& ui, CFDMesh& cfd_mesh) {
    begin_component(ui, "Mesh");
    {
        field(ui, "color", &cfd_mesh.color);
        field(ui, "mesh", &cfd_mesh.model);
    }
    end_component(ui);
}

void render_domain_component(UI& ui, CFDDomain& domain) {
    begin_component(ui, "Domain");
        field(ui, "size", &domain.size);
    end_component(ui);

    begin_component(ui, "Boundary Layer");
        field(ui, "layers", &domain.contour_layers);
        field(ui, "initial thickness", &domain.contour_initial_thickness);
        field(ui, "exponent", &domain.contour_thickness_expontent);
    end_component(ui);

    begin_component(ui, "Tetrahedron Layer");
        field(ui, "layers", &domain.tetrahedron_layers);
    end_component(ui);
    
    begin_component(ui, "Grid");
    field(ui, "resolution", &domain.grid_resolution);
    field(ui, "high layers", &domain.grid_layers);
    end_component(ui);
    
    begin_component(ui, "Visualization");
    field(ui, "center", &domain.center);
    field(ui, "plane", &domain.plane);
    end_component(ui);

    begin_component(ui, "Hex Mesh");
    field(ui, "quad quality", &domain.quad_quality);
    field(ui, "feature angle", &domain.feature_angle);
    field(ui, "feature quality", &domain.min_feature_quality);
    end_component(ui);
}

void render_inspector(Inspector& inspector) {
    UI& ui = inspector.ui;
    World& world = inspector.world;

    begin_vstack(ui).padding(0);

    if (inspector.selection.active()) {
        ID selected = inspector.selection.get_active();

        sstring& name = name_of_entity(inspector.lister, selected);
    
        input(ui, &name).width({Perc,100}).margin(4);
        divider(ui, shade1);

        Archetype arch = world.arch_of_id(selected);

        if (Transform* trans = world.m_by_id<Transform>(selected); trans) {
            render_transform_component(ui, *trans);
        }

        if (Camera* camera = world.m_by_id<Camera>(selected); camera) {
            render_camera_component(ui, *camera);
        }

        if (CFDMesh* mesh = world.m_by_id<CFDMesh>(selected); mesh) {
            render_mesh_component(ui, *mesh);
        }

        if (CFDDomain* domain = world.m_by_id<CFDDomain>(selected); domain) {
            render_domain_component(ui, *domain);
        }
    }

    end_vstack(ui);
}
