#include "ecs/ecs.h"
#include "graphics/assets/assets.h"
#include "graphics/renderer/renderer.h"
#include "graphics/rhi/draw.h"
#include "graphics/rhi/frame_buffer.h"
#include "components/lights.h"
#include "graphics/renderer/ibl.h"
#include "components/transform.h"
#include "components/camera.h"

void fill_pass_ubo(PassUBO& pass_ubo, const Viewport& viewport) {
	pass_ubo.proj = viewport.proj;
	pass_ubo.view = viewport.view;
	pass_ubo.resolution.x = viewport.width;
	pass_ubo.resolution.y = viewport.height;

#ifdef RENDER_API_VULKAN
	pass_ubo.proj[1][1] *= -1;
#endif
}

 struct LightingUBO {
	alignas(16) glm::vec3 view_pos;
	alignas(16) glm::vec3 dir_light_direction;
	alignas(16) glm::vec3 dir_light_color;
};


#include "graphics/rhi/primitives.h"
#include "components/camera.h"
