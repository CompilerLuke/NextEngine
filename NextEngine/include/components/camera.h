#pragma once

#include "ecs/ecs.h"
#include "reflection/reflection.h"

struct Camera {
	float near_plane = 0.1f;
	float far_plane = 600;
	float fov = 60;
	void update_matrices(World&, RenderParams&);

	REFLECT()
};

struct CameraSystem : System {
	void render(World&, RenderParams&) override;
};

