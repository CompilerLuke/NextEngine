#pragma once

#include "core/container/handle_manager.h"
#include "graphics/renderer/material_system.h"

struct MaterialManager : HandleManager<struct Material, material_handle> {

};

namespace RHI {
	extern MaterialManager ENGINE_API material_manager;
}