// TheUnpluggingGame.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "game.h"

#include <ecs/system.h>
#include <components/transform.h>
#include <model/model.h>
#include <graphics/rhi.h>

#include "playerInput.h"
#include "fpsController.h"

DEFINE_GAME_COMPONENT_ID(PlayerInput, 50);
DEFINE_GAME_COMPONENT_ID(FPSController, 51);

extern "C" {
	void register_components_and_systems(World& world) {
		register_default_systems_and_components(world);

		world.add(new Store<PlayerInput>(1));
		world.add(new Store<FPSController>(10));
		
		world.add(new PlayerInputSystem());
		world.add(new FPSControllerSystem());

		Handle<Model> sphere = load_Model("cube.fbx");
		Handle<Shader> shader = load_Shader("shaders/pbr.vert", "shaders/pbr.frag");


		Handle<Material> mat_handle = RHI::material_manager.make({
			shader,
			{},
			&default_draw_state
			});
	}
}