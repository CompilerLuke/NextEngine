#pragma once

#include "core/container/handle_manager.h"
#include "core/handle.h"
#include "graphics/assets/material.h"

struct ENGINE_API MaterialManager : HandleManager<Material, material_handle> {

};