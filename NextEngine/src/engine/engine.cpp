#include "stdafx.h"
#include "engine/engine.h"
#include "graphics/renderer.h"
#include "core/input.h"

Engine::Engine() : input(window) {
	window.init();

	register_default_systems_and_components(world);
}