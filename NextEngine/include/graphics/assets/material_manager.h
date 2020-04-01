#pragma once

#include "core/container/handle_manager.h"
#include "core/handle.h"
#include "graphics/renderer/material_system.h"

struct MaterialManager : HandleManager<Material, material_handle> {

};