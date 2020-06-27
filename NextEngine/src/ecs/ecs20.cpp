/*
#include "ecs/ecs20.h"
#include "components/transform.h"
#include "components/camera.h"

namespace ecs {
	void test() {
		World world;
		auto [trans, ball] = world.make<Transform, Camera>();

		trans.position.x = 600;
		trans.position.y = 43;

		for (auto [trans, camera] : world.filter<const Transform, Camera>()) {
			camera.far_plane = 10;
		}
	}
}
*/