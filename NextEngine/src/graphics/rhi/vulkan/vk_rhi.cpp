#include "graphics/rhi/rhi.h"
#include "graphics/rhi/vulkan/vulkan.h"

/*

RHI* make_RHI(AppInfo& info, DeviceFeatures& features) {
const char* validation_layers[1] = {
"VK_LAYER_KHRONOS_validation"
};

VulkanDesc vk_desc = {};
vk_desc.api_version = VK_MAKE_VERSION(1, 0, 0);
vk_desc.app_name = info.app_name;
vk_desc.app_version = VK_MAKE_VERSION(0, 0, 0);
vk_desc.engine_name = "NextEngine";
vk_desc.engine_version = VK_MAKE_VERSION(0, 0, 0);
vk_desc.min_log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
vk_desc.validation_layers = validation_layers;
vk_desc.num_validation_layers = 1;
vk_desc.device_features.samplerAnisotropy = features.sampler_anistropy;
vk_desc.device_features.multiDrawIndirect = features.multi_draw_indirect;

//vk_make_RHI(vk_desc);
}

*/