#include "editor/viewport.h"
#include "ecs/ecs.h"
#include "components/transform.h"

SceneViewport::SceneViewport(Window& window) {
	input.init(window);
}

void SceneViewport::update(glm::vec2 offset, glm::vec2 size) {
	input.region_min = offset;
	input.region_max = offset + size;

	viewport.width = size.x;
	viewport.height = size.y;
}